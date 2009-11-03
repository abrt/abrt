#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "abrtlib.h"
#include "Catcut.h"
#include "CrashTypes.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

using namespace std;

static xmlrpc_env env;
static xmlrpc_client* client = NULL;
static struct xmlrpc_clientparms clientParms;
static struct xmlrpc_curl_xportparms curlParms;
static xmlrpc_server_info* server_info = NULL;


static string login(const char* login, const char* passwd);
//static void logout();
static void new_xmlrpc_client(const char* url, bool no_ssl_verify);
static void destroy_xmlrpc_client();
static void create_new_bug_description(const map_crash_report_t& pCrashReport, string& pDescription);
static void get_product_and_version(const string& pRelease,
                                          string& pProduct,
                                          string& pVersion);


static void throw_if_xml_fault_occurred()
{
    if (env.fault_occurred)
    {
        string errmsg = ssprintf("XML-RPC Fault: %s(%d)", env.fault_string, env.fault_code);
        error_msg("%s", errmsg.c_str()); // show error in daemon log
        throw CABRTException(EXCEP_PLUGIN, errmsg);
    }
}

static void new_xmlrpc_client(const char* url, bool no_ssl_verify)
{
    xmlrpc_env_init(&env);

    /* This should be done at program startup, once.
     * We do it in abrtd's main */
    /* xmlrpc_client_setup_global_const(&env); */

    curlParms.network_interface = NULL;
    curlParms.no_ssl_verifypeer = no_ssl_verify;
    curlParms.no_ssl_verifyhost = no_ssl_verify;
#ifdef VERSION
    curlParms.user_agent        = PACKAGE_NAME"/"VERSION;
#else
    curlParms.user_agent        = "abrt";
#endif

    clientParms.transport          = "curl";
    clientParms.transportparmsP    = &curlParms;
    clientParms.transportparm_size = XMLRPC_CXPSIZE(user_agent);

    xmlrpc_client_create(&env, XMLRPC_CLIENT_NO_FLAGS, PACKAGE_NAME, VERSION, &clientParms, XMLRPC_CPSIZE(transportparm_size),
                         &client);
    throw_if_xml_fault_occurred();

    server_info = xmlrpc_server_info_new(&env, url);
    throw_if_xml_fault_occurred();
}

static void destroy_xmlrpc_client()
{
    xmlrpc_server_info_free(server_info);
    xmlrpc_env_clean(&env);
    xmlrpc_client_destroy(client);
}

static string login(const char* login, const char* passwd)
{
    xmlrpc_value* param = xmlrpc_build_value(&env, "(ss)", login, passwd);
    throw_if_xml_fault_occurred();

    xmlrpc_value* result;
    xmlrpc_client_call2(&env, client, server_info, "Catcut.auth", param, &result);
    throw_if_xml_fault_occurred();
    xmlrpc_DECREF(param);

    xmlrpc_value *cookie_xml;
    const char *cookie;
    string cookie_str;
    xmlrpc_struct_find_value(&env, result, "cookie", &cookie_xml);
    throw_if_xml_fault_occurred();
    xmlrpc_read_string(&env, cookie_xml, &cookie);
    throw_if_xml_fault_occurred();
    cookie_str = cookie;
    /* xmlrpc_read_string returns *malloc'ed ptr*.
     * doc is not very clear on it, but I looked in xmlrpc sources. */
    free((void*)cookie);
    xmlrpc_DECREF(cookie_xml);

    xmlrpc_DECREF(result);

    return cookie_str;
}

// catcut does not have it (yet?)
//static void logout()
//{
//    xmlrpc_value* param = xmlrpc_build_value(&env, "(s)", "");
//    throw_if_xml_fault_occurred();
//
//    xmlrpc_value* result = NULL; /* paranoia */
//    xmlrpc_client_call2(&env, client, server_info, "User.logout", param, &result);
//    throw_if_xml_fault_occurred();
//}

static void create_new_bug_description(const map_crash_report_t& pCrashReport, string& pDescription)
{
    string howToReproduce;
    string comment;

    if (pCrashReport.find(CD_REPRODUCE) != pCrashReport.end())
    {
        howToReproduce = "\n\nHow to reproduce\n"
                         "-----\n" +
                         pCrashReport.find(CD_REPRODUCE)->second[CD_CONTENT];
    }
    if (pCrashReport.find(CD_COMMENT) != pCrashReport.end())
    {
        comment = "\n\nComment\n"
                 "-----\n" +
                 pCrashReport.find(CD_COMMENT)->second[CD_CONTENT];
    }
    pDescription = "\nabrt detected a crash.\n" +
                   howToReproduce +
                   comment +
                   "\n\nAdditional information\n"
                   "======\n";

    map_crash_report_t::const_iterator it;
    for (it = pCrashReport.begin(); it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT)
        {
            if (it->first != CD_UUID &&
                it->first != FILENAME_ARCHITECTURE &&
                it->first != FILENAME_RELEASE &&
                it->first != CD_REPRODUCE &&
                it->first != CD_COMMENT)
            {
                pDescription += "\n" + it->first + "\n";
                pDescription += "-----\n";
                pDescription += it->second[CD_CONTENT] + "\n\n";
            }
        }
        else if (it->second[CD_TYPE] == CD_ATT)
        {
            pDescription += "\n\nAttached files\n"
                            "----\n";
            pDescription += it->first + "\n";
        }
        else if (it->second[CD_TYPE] == CD_BIN)
        {
            string msg = ssprintf(_("Binary file %s will not be reported."), it->first.c_str());
            warn_client(msg);
            //update_client(_("Binary file ")+it->first+_(" will not be reported."));
        }
    }
}

static void get_product_and_version(const string& pRelease,
                                          string& pProduct,
                                          string& pVersion)
{
    if (pRelease.find("Rawhide") != string::npos)
    {
        pProduct = "Fedora";
        pVersion = "rawhide";
        return;
    }
    if (pRelease.find("Fedora") != string::npos)
    {
        pProduct = "Fedora";
    }
    else if (pRelease.find("Red Hat Enterprise Linux") != string::npos)
    {
        pProduct = "Red Hat Enterprise Linux ";
    }
    string::size_type pos = pRelease.find("release");
    pos = pRelease.find(" ", pos) + 1;
    while (pRelease[pos] != ' ')
    {
        pVersion += pRelease[pos];
        if (pProduct == "Red Hat Enterprise Linux ")
        {
            pProduct += pRelease[pos];
        }
        pos++;
    }
}

static string new_bug(const char *auth_cookie, const map_crash_report_t& pCrashReport)
{
    string package = pCrashReport.find(FILENAME_PACKAGE)->second[CD_CONTENT];
    string component = pCrashReport.find(FILENAME_COMPONENT)->second[CD_CONTENT];
    string release = pCrashReport.find(FILENAME_RELEASE)->second[CD_CONTENT];
    string arch = pCrashReport.find(FILENAME_ARCHITECTURE)->second[CD_CONTENT];
    string uuid = pCrashReport.find(CD_UUID)->second[CD_CONTENT];

    string summary = "[abrt] crash detected in " + package;
    string status_whiteboard = "abrt_hash:" + uuid;

    string description;
    create_new_bug_description(pCrashReport, description);

    string product;
    string version;
    get_product_and_version(release, product, version);

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
    throw_if_xml_fault_occurred();

    xmlrpc_value *result;
    xmlrpc_client_call2(&env, client, server_info, "Catcut.createTicket", param, &result);
    throw_if_xml_fault_occurred();
    xmlrpc_DECREF(param);

    xmlrpc_value *bug_id_xml;
    const char *bug_id;
    string bug_id_str;
    xmlrpc_struct_find_value(&env, result, "ticket", &bug_id_xml);
    throw_if_xml_fault_occurred();
    xmlrpc_read_string(&env, bug_id_xml, &bug_id);
    throw_if_xml_fault_occurred();
    bug_id_str = bug_id;
    log("New bug id: %s", bug_id);
    update_client(_("New bug id: ") + bug_id_str);
    free((void*)bug_id);
    xmlrpc_DECREF(bug_id_xml);

    xmlrpc_DECREF(result);

    return bug_id_str;
}

//static
//void add_attachments(const string& pBugId, const map_crash_report_t& pCrashReport)
//{
//    xmlrpc_value* result = NULL;
//
//    map_crash_report_t::const_iterator it = pCrashReport.begin();
//    for (; it != pCrashReport.end(); it++)
//    {
//        if (it->second[CD_TYPE] == CD_ATT)
//        {
//            string description = "File: " + it->first;
//            const string& to_encode = it->second[CD_CONTENT];
//            char *encoded64 = encode_base64(to_encode.c_str(), to_encode.length());
//            xmlrpc_value* param = xmlrpc_build_value(&env,"(s{s:s,s:s,s:s,s:s})",
//                                              pBugId.c_str(),
//                                              "description", description.c_str(),
//                                              "filename", it->first.c_str(),
//                                              "contenttype", "text/plain",
//                                              "data", encoded64
//                                      );
//            free(encoded64);
//            throw_if_xml_fault_occurred();
//
//// catcut has this API:
//// struct response requestUpload(string cookie, string ticket, string filename, string description)
////response MUST include "errno", "errmsg" members; if an upload is approved,
////a "URL" MUST be returned in the response. The description string
////should include a brief description of the file.
////
////The client should upload the file via HTTP PUT to the provided
////URL.  The provided URL may be absolute or relative, if relative it must
////be combined with the base URL of the XML-RPC server using the usual
////rules for relative URL's (RFC 3986).
//            xmlrpc_client_call2(&env, client, server_info, "catcut.addAttachment", param, &result);
//            throw_if_xml_fault_occurred();
//        }
//    }
//}

CReporterCatcut::CReporterCatcut() :
    m_sCatcutURL("http://127.0.0.1:8080/catcut/xmlrpc"),
    m_bNoSSLVerify(false)
{}

CReporterCatcut::~CReporterCatcut()
{}

string CReporterCatcut::Report(const map_crash_report_t& pCrashReport,
                               const map_plugin_settings_t& pSettings, const string& pArgs)
{
    update_client(_("Creating new bug..."));
    try
    {
        new_xmlrpc_client(m_sCatcutURL.c_str(), m_bNoSSLVerify);
        string auth_cookie = login(m_sLogin.c_str(), m_sPassword.c_str());
        string bug_id = (auth_cookie != "") ? new_bug(auth_cookie.c_str(), pCrashReport) : "";
//        add_attachments(to_string(bug_id), pCrashReport);
//        update_client(_("Logging out..."));
//        logout();
        destroy_xmlrpc_client();
        return "New catcut bug ID: " + bug_id;

    }
    catch (CABRTException& e)
    {
        destroy_xmlrpc_client();
        throw CABRTException(EXCEP_PLUGIN, string("CReporterCatcut::Report(): ") + e.what());
    }
}

void CReporterCatcut::SetSettings(const map_plugin_settings_t& pSettings)
{
    map_plugin_settings_t::const_iterator it;
    map_plugin_settings_t::const_iterator end = pSettings.end();

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
        m_bNoSSLVerify = it->second == "yes";
    }
}

map_plugin_settings_t CReporterCatcut::GetSettings()
{
    map_plugin_settings_t ret;

    ret["CatcutURL"] = m_sCatcutURL;
    ret["Login"] = m_sLogin;
    ret["Password"] = m_sPassword;
    ret["NoSSLVerify"] = m_bNoSSLVerify ? "yes" : "no";

    return ret;
}

PLUGIN_INFO(REPORTER,
            CReporterCatcut,
            "Catcut",
            "0.0.1",
            "Test plugin to report bugs to catcut and if not, report it.",
            "dvlasenk@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Catcut.GTKBuilder");
