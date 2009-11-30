#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <curl/curl.h>
#include "abrtlib.h"
#include "abrt_xmlrpc.h"
#include "Catcut.h"
#include "CrashTypes.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

using namespace std;


static int
put_stream(const char *pURL, FILE* f, size_t content_length)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        throw CABRTException(EXCEP_PLUGIN, "put_stream: can't initialize curl library");
    }
    /* enable uploading */
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    /* specify target */
    curl_easy_setopt(curl, CURLOPT_URL, pURL);
    /* file handle: passed to the default callback, it will fread() it */
    curl_easy_setopt(curl, CURLOPT_READDATA, f);
    /* get file size */
    curl_easy_setopt(curl, CURLOPT_INFILESIZE, content_length);
    /* everything is done here; result 0 means success */
    int result = curl_easy_perform(curl);
    /* goodbye */
    curl_easy_cleanup(curl);
    return result;
}

static void
send_string(const char *pURL,
            const char *pContent,
            int retryCount,
            int retryDelaySeconds)
{
    if (pURL[0] == '\0')
    {
        error_msg(_("send_string: URL not specified"));
        return;
    }

    do
    {
        int content_length = strlen(pContent);
        FILE* f = fmemopen((void*)pContent, content_length, "r");
        if (!f)
        {
            throw CABRTException(EXCEP_PLUGIN, "send_string: can't open string stream");
        }
        int result = put_stream(pURL, f, content_length);
        fclose(f);
        if (!result)
            return;
        update_client(_("Sending failed, try it again: %s"), curl_easy_strerror((CURLcode)result));
    }
    /*retry the upload if not succesful, wait a bit before next try*/
    while (--retryCount != 0 && (sleep(retryDelaySeconds), 1));

    throw CABRTException(EXCEP_PLUGIN, "send_string: can't send string");
}

static void
send_file(const char *pURL,
          const char *pFilename,
          int retryCount,
          int retryDelaySeconds)
{
    if (pURL[0] == '\0')
    {
        error_msg(_("send_file: URL not specified"));
        return;
    }

    update_client(_("Sending file %s to %s"), pFilename, pURL);

    do
    {
        FILE* f = fopen(pFilename, "r");
        if (!f)
        {
            throw CABRTException(EXCEP_PLUGIN, "send_file: can't open string stream");
        }
        struct stat buf;
        fstat(fileno(f), &buf); /* can't fail */
        int content_length = buf.st_size;
        int result = put_stream(pURL, f, content_length);
        fclose(f);
        if (!result)
            return;
        update_client(_("Sending failed, try it again: %s"), curl_easy_strerror((CURLcode)result));
    }
    /*retry the upload if not succesful, wait a bit before next try*/
    while (--retryCount != 0 && (sleep(retryDelaySeconds), 1));

    throw CABRTException(EXCEP_PLUGIN, "send_file: can't send file");
}

static string
resolve_relative_url(const char *url, const char *base)
{
    // if 'url' is relative (not absolute) combine it with 'base'
    //    (which must be absolute)
    // Only works in limited cases:
    //     0) url is already absolute
    //     1) url starts with two slashes
    //     2) url starts with one slash

    const char *colon = strchr(url, ':');
    const char *slash = strchr(url, '/');

    if (colon && (!slash || colon < slash))
    {
        return url;
    }

    const char *end_of_protocol = strchr(base, ':');
    string protocol(base, end_of_protocol - base);

    end_of_protocol += 3; /* skip "://" */
    const char *end_of_host = strchr(end_of_protocol, '/');
    string host(end_of_protocol, end_of_host - end_of_protocol);

    if (url[0] == '/')
    {
        if (url[1] == '/')
        {
            protocol += ':';
            protocol += url;
            return protocol;
        }
        protocol += "://";
        protocol += host;
        protocol += url;
        return protocol;
    }
    throw CABRTException(EXCEP_PLUGIN, "resolve_relative_url: unhandled relative url");
}

//
// struct_find_XXXX
//   abstract all the busy work of getting a field's value from
//   a struct.   XXXX is a type.
//   Return true/false = the field is in the struct
//   If true, return the field's value in 'value'.
//
//   This function currently just assumes that the value in the
//   field can be read into the type of 'value'.  This should probably
//   be fixed to either convert the fields value to the type of 'value'
//   or error specifically/usefully.
//
//   This function probably should be converted to an overloaded function
//   (overloaded on the type of 'value').  It could also be a function
//   template.
//

static bool
struct_find_int(xmlrpc_env* env, xmlrpc_value* result,
                       const char* fieldName, int& value)
{
    xmlrpc_value* an_xmlrpc_value;
    xmlrpc_struct_find_value(env, result, fieldName, &an_xmlrpc_value);
    throw_if_xml_fault_occurred(env);
    if (an_xmlrpc_value)
    {
        xmlrpc_read_int(env, an_xmlrpc_value, &value);
        throw_if_xml_fault_occurred(env);
        xmlrpc_DECREF(an_xmlrpc_value);
        return true;
    }
    return false;
}

static bool
struct_find_string(xmlrpc_env* env, xmlrpc_value* result,
                          const char* fieldName, string& value)
{
    xmlrpc_value* an_xmlrpc_value;
    xmlrpc_struct_find_value(env, result, fieldName, &an_xmlrpc_value);
    throw_if_xml_fault_occurred(env);
    if (an_xmlrpc_value)
    {
        const char* value_s;
        xmlrpc_read_string(env, an_xmlrpc_value, &value_s);
        throw_if_xml_fault_occurred(env);
        value = value_s;
        xmlrpc_DECREF(an_xmlrpc_value);
        free((void*)value_s);
        return true;
    }
    return false;
}


/*
 * Static namespace for xmlrpc stuff.
 * Used mainly to ensure we always destroy xmlrpc client and server_info.
 */

namespace {

struct ctx: public abrt_xmlrpc_conn {
    ctx(const char* url, bool no_ssl_verify): abrt_xmlrpc_conn(url, no_ssl_verify) {}

    string login(const char* login, const char* passwd);
    string new_bug(const char *auth_cookie, const map_crash_report_t& pCrashReport);
    string request_upload(const char* auth_cookie, const char* pTicketName,
                const char* fileName, const char* description);
    void add_attachments(const char* xmlrpc_URL,
                const char* auth_cookie,
                const char* pTicketName,
                const map_crash_report_t& pCrashReport,
                int retryCount,
                int retryDelaySeconds);
};

string
ctx::login(const char* login, const char* passwd)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "(ss)", login, passwd);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result;
    xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "Catcut.auth", param, &result);
    xmlrpc_DECREF(param);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value *cookie_xml;
    const char *cookie;
    string cookie_str;
    xmlrpc_struct_find_value(&env, result, "cookie", &cookie_xml);
    throw_if_xml_fault_occurred(&env);
    xmlrpc_read_string(&env, cookie_xml, &cookie);
    throw_if_xml_fault_occurred(&env);
    cookie_str = cookie;
    /* xmlrpc_read_string returns *malloc'ed ptr*.
     * doc is not very clear on it, but I looked in xmlrpc sources. */
    free((void*)cookie);
    xmlrpc_DECREF(cookie_xml);

    xmlrpc_DECREF(result);

    return cookie_str;
}

string
ctx::new_bug(const char *auth_cookie, const map_crash_report_t& pCrashReport)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    string package = pCrashReport.find(FILENAME_PACKAGE)->second[CD_CONTENT];
    string component = pCrashReport.find(FILENAME_COMPONENT)->second[CD_CONTENT];
    string release = pCrashReport.find(FILENAME_RELEASE)->second[CD_CONTENT];
    string arch = pCrashReport.find(FILENAME_ARCHITECTURE)->second[CD_CONTENT];
    string uuid = pCrashReport.find(CD_UUID)->second[CD_CONTENT];

    string summary = "[abrt] crash in " + package;
    string status_whiteboard = "abrt_hash:" + uuid;

    string description = make_description_catcut(pCrashReport);

    string product;
    string version;
    parse_release(release.c_str(), product, version);

    xmlrpc_value *param = xmlrpc_build_value(&env, "(s{s:s,s:s,s:s,s:s,s:s,s:s,s:s})",
                auth_cookie,
                "product", product.c_str(),
                "component", component.c_str(),
                "version", version.c_str(),
                "summary", summary.c_str(),
                "description", description.c_str(),
                "status_whiteboard", status_whiteboard.c_str(),
                "platform", arch.c_str()
                );
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value *result;
    xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "Catcut.createTicket", param, &result);
    xmlrpc_DECREF(param);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value *bug_id_xml;
    const char *bug_id;
    string bug_id_str;
    xmlrpc_struct_find_value(&env, result, "ticket", &bug_id_xml);
    throw_if_xml_fault_occurred(&env);
    xmlrpc_read_string(&env, bug_id_xml, &bug_id);
    throw_if_xml_fault_occurred(&env);
    bug_id_str = bug_id;
    log("New bug id: %s", bug_id);
    update_client(_("New bug id: %s"), bug_id);
    free((void*)bug_id);
    xmlrpc_DECREF(bug_id_xml);

    xmlrpc_DECREF(result);

    return bug_id_str;
}

string
ctx::request_upload(const char* auth_cookie, const char* pTicketName,
               const char* fileName, const char* description)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "(ssss)",
                auth_cookie,
                pTicketName,
                fileName,
                description);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "Catcut.requestUpload", param, &result);
    xmlrpc_DECREF(param);
    throw_if_xml_fault_occurred(&env);

    string URL;
    bool has_URL = struct_find_string(&env, result, "uri", URL);
    if (!has_URL || URL == "")
    {
        int err;
        bool has_errno = struct_find_int(&env, result, "errno", err);
        if (has_errno && err)
        {
            string errmsg;
            bool has_errmsg = struct_find_string(&env, result, "errmsg", errmsg);
            if (has_errmsg)
            {
                log("error returned by requestUpload: %s", errmsg.c_str());
                update_client(_("error returned by requestUpload: %s"), errmsg.c_str());
            }
            else
            {
                log("error returned by requestUpload: %d", err);
                update_client(_("error returned by requestUpload: %d"), err);
            }
        }
        else
        {
            log("no URL returned by requestUpload, and no err");
            update_client(_("no URL returned by requestUpload, and no errno"));
        }
    }

    log("requestUpload returned URL: %s", URL.c_str());
    update_client(_("requestUpload returned URL: %s"), URL.c_str());

    xmlrpc_DECREF(result);
    return URL;
}

void
ctx::add_attachments(const char* xmlrpc_URL,
                const char* auth_cookie,
                const char* pTicketName,
                const map_crash_report_t& pCrashReport,
                int retryCount,
                int retryDelaySeconds)
{

    map_crash_report_t::const_iterator it = pCrashReport.begin();
    for (; it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_ATT)
        {
            update_client(_("Attaching (CD_ATT): %s"), it->first.c_str());

            string description = "File: " + it->first;
            string URL = request_upload(auth_cookie,
                        pTicketName,
                        it->first.c_str(),
                        description.c_str());

            URL = resolve_relative_url(URL.c_str(), xmlrpc_URL);

            log("rebased URL: %s", URL.c_str());
            update_client(_("rebased URL: %s"), URL.c_str());

            send_string(URL.c_str(), it->second[CD_CONTENT].c_str(),
                        retryCount, retryDelaySeconds);
        }
        else if (it->second[CD_TYPE] == CD_BIN)
        {
            update_client(_("Attaching (CD_ATT): %s"), it->first.c_str());

            string description = "File: " + it->first;
            string URL = request_upload(auth_cookie,
                                        pTicketName,
                                        it->first.c_str(),
                                        description.c_str());

            URL = resolve_relative_url(URL.c_str(), xmlrpc_URL);

            log("rebased URL: %s", URL.c_str());
            update_client(_("rebased URL: %s"), URL.c_str());

            send_file(URL.c_str(), it->second[CD_CONTENT].c_str(),
                      retryCount, retryDelaySeconds);
        }
    }
}

} /* namespace */


/*
 * CReporterCatcut
 */

CReporterCatcut::CReporterCatcut() :
    m_sCatcutURL("http://127.0.0.1:8080/catcut/xmlrpc"),
    m_bNoSSLVerify(false),
    m_nRetryCount(3),
    m_nRetryDelay(20)
{}

CReporterCatcut::~CReporterCatcut()
{}

string CReporterCatcut::Report(const map_crash_report_t& pCrashReport,
                               const map_plugin_settings_t& pSettings,
                               const string& pArgs)
{
    update_client(_("Creating new bug..."));
    try
    {
        ctx catcut_server(m_sCatcutURL.c_str(), m_bNoSSLVerify);

        string auth_cookie = catcut_server.login(m_sLogin.c_str(), m_sPassword.c_str());
        string message;
        if (auth_cookie != "")
        {
            string ticket_name = catcut_server.new_bug(auth_cookie.c_str(), pCrashReport);
            if (ticket_name != "")
            {
                catcut_server.add_attachments(
                        m_sCatcutURL.c_str(),
                        auth_cookie.c_str(),
                        ticket_name.c_str(),
                        pCrashReport,
                        m_nRetryCount,
                        m_nRetryDelay
                );
                message = "New catcut bug ID: " + ticket_name;
            }
            else
            {
                message = "Error: can't create ticket";
            }
        }
        else
        {
            message = "Error: can't create ticket";
        }
        return message;
    }
    catch (CABRTException& e)
    {
        throw CABRTException(EXCEP_PLUGIN, e.what());
    }
}

void CReporterCatcut::SetSettings(const map_plugin_settings_t& pSettings)
{
    m_pSettings = pSettings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
    it = pSettings.find("CatcutURL");
    if (it != end)
    {
        m_sCatcutURL = it->second;
    }
    it = pSettings.find("Login");
    if (it != end)
    {
        m_sLogin = it->second;
    }
    it = pSettings.find("Password");
    if (it != end)
    {
        m_sPassword = it->second;
    }
    it = pSettings.find("NoSSLVerify");
    if (it != end)
    {
        m_bNoSSLVerify = string_to_bool(it->second.c_str());
    }
    it = pSettings.find("RetryCount");
    if (it != end)
    {
        m_nRetryCount = atoi(it->second.c_str());
    }
    it = pSettings.find("RetryDelay");
    if (it != end)
    {
        m_nRetryDelay = atoi(it->second.c_str());
    }
}

PLUGIN_INFO(REPORTER,
            CReporterCatcut,
            "Catcut",
            "0.0.1",
            "Reports bugs to catcut",
            "dvlasenk@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Catcut.GTKBuilder");
