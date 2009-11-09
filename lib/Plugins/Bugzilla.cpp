#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include "abrtlib.h"
#include "Bugzilla.h"
#include "CrashTypes.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#define XML_RPC_SUFFIX "/xmlrpc.cgi"


static void get_product_and_version(const std::string& pRelease,
                                          std::string& pProduct,
                                          std::string& pVersion)
{
    if (pRelease.find("Rawhide") != std::string::npos)
    {
        pProduct = "Fedora";
        pVersion = "rawhide";
        return;
    }
    if (pRelease.find("Fedora") != std::string::npos)
    {
        pProduct = "Fedora";
    }
    else if (pRelease.find("Red Hat Enterprise Linux") != std::string::npos)
    {
        pProduct = "Red Hat Enterprise Linux ";
    }
    std::string::size_type pos = pRelease.find("release");
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

static void create_new_bug_description(const map_crash_report_t& pCrashReport, std::string& pDescription)
{
    pDescription = "abrt detected a crash.\n\n";
    pDescription += make_description_bz(pCrashReport);
}

// FIXME: we still leak memory if this function detects a fault:
// many instances when we leave non-freed or non-xmlrpc_DECREF'ed data behind.
static void throw_if_xml_fault_occurred(xmlrpc_env *env)
{
    if (env->fault_occurred)
    {
        std::string errmsg = ssprintf("XML-RPC Fault: %s(%d)", env->fault_string, env->fault_code);
        xmlrpc_env_clean(env); // this is needed ONLY if fault_occurred
        xmlrpc_env_init(env); // just in case user catches ex and _continues_ to use env
        error_msg("%s", errmsg.c_str()); // show error in daemon log
        throw CABRTException(EXCEP_PLUGIN, errmsg);
    }
}


/*
 * Static namespace for xmlrpc stuff.
 * Used mainly to ensure we always destroy xmlrpc client and server_info.
 */

namespace {

struct ctx {
	xmlrpc_client* client;
	xmlrpc_server_info* server_info;

        ctx(const char* url, bool no_ssl_verify) { new_xmlrpc_client(url, no_ssl_verify); }
	~ctx() { destroy_xmlrpc_client(); }

	void new_xmlrpc_client(const char* url, bool no_ssl_verify);
	void destroy_xmlrpc_client();

	void login(const char* login, const char* passwd);
	void logout();
	int32_t check_uuid_in_bugzilla(const char* component, const char* UUID);
	bool check_cc_and_reporter(uint32_t bug_id, const char* login);
	void add_plus_one_cc(uint32_t bug_id, const char* login);
	uint32_t new_bug(const map_crash_report_t& pCrashReport);
	void add_attachments(const char* bug_id_str, const map_crash_report_t& pCrashReport);
};

void ctx::new_xmlrpc_client(const char* url, bool no_ssl_verify)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    /* This should be done at program startup, once.
     * We do it in abrtd's main */
    /* xmlrpc_client_setup_global_const(&env); */

    struct xmlrpc_curl_xportparms curlParms;
    memset(&curlParms, 0, sizeof(curlParms));
    /* curlParms.network_interface = NULL; - done by memset */
    curlParms.no_ssl_verifypeer = no_ssl_verify;
    curlParms.no_ssl_verifyhost = no_ssl_verify;
#ifdef VERSION
    curlParms.user_agent        = PACKAGE_NAME"/"VERSION;
#else
    curlParms.user_agent        = "abrt";
#endif

    struct xmlrpc_clientparms clientParms;
    memset(&clientParms, 0, sizeof(clientParms));
    clientParms.transport          = "curl";
    clientParms.transportparmsP    = &curlParms;
    clientParms.transportparm_size = XMLRPC_CXPSIZE(user_agent);

    client = NULL;
    xmlrpc_client_create(&env, XMLRPC_CLIENT_NO_FLAGS,
                        PACKAGE_NAME, VERSION,
                        &clientParms, XMLRPC_CPSIZE(transportparm_size),
                        &client);
    throw_if_xml_fault_occurred(&env);

    server_info = xmlrpc_server_info_new(&env, url);
    if (env.fault_occurred)
    {
        xmlrpc_client_destroy(client);
        client = NULL;
    }
    throw_if_xml_fault_occurred(&env);
}

void ctx::destroy_xmlrpc_client()
{
    if (server_info)
    {
        xmlrpc_server_info_free(server_info);
        server_info = NULL;
    }
    if (client)
    {
        xmlrpc_client_destroy(client);
        client = NULL;
    }
}

void ctx::login(const char* login, const char* passwd)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:s,s:s})", "login", login, "password", passwd);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "User.login", param, &result);
    xmlrpc_DECREF(param);
    if (result)
        xmlrpc_DECREF(result);

    if (env.fault_occurred)
    {
        std::string errmsg = ssprintf("Can't login. Check Edit->Plugins->Bugzilla and /etc/abrt/plugins/Bugzilla.conf. Server said: %s", env.fault_string);
        xmlrpc_env_clean(&env);
        error_msg("%s", errmsg.c_str()); // show error in daemon log
        throw CABRTException(EXCEP_PLUGIN, errmsg);
    }
}

void ctx::logout()
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "(s)", "");
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "User.logout", param, &result);
    xmlrpc_DECREF(param);
    if (result)
        xmlrpc_DECREF(result);
    throw_if_xml_fault_occurred(&env);
}

bool ctx::check_cc_and_reporter(uint32_t bug_id, const char* login)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "(s)", to_string(bug_id).c_str());
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "bugzilla.getBug", param, &result);
    throw_if_xml_fault_occurred(&env);
    xmlrpc_DECREF(param);

    xmlrpc_value* reporter_member = NULL;
    xmlrpc_struct_find_value(&env, result, "reporter", &reporter_member);
    throw_if_xml_fault_occurred(&env);

    if (reporter_member)
    {
        const char* reporter = NULL;
        xmlrpc_read_string(&env, reporter_member, &reporter);
        throw_if_xml_fault_occurred(&env);

        bool eq = (strcmp(reporter, login) == 0);
        free((void*)reporter);
        xmlrpc_DECREF(reporter_member);
        if (eq)
        {
            xmlrpc_DECREF(result);
            return true;
        }
    }

    xmlrpc_value* cc_member = NULL;
    xmlrpc_struct_find_value(&env, result, "cc", &cc_member);
    throw_if_xml_fault_occurred(&env);

    if (cc_member)
    {
        uint32_t array_size = xmlrpc_array_size(&env, cc_member);

        for (uint32_t i = 0; i < array_size; i++)
        {
            xmlrpc_value* item = NULL;
            xmlrpc_array_read_item(&env, cc_member, i, &item); // Correct
            throw_if_xml_fault_occurred(&env);

            const char* cc = NULL;
            xmlrpc_read_string(&env, item, &cc);
            throw_if_xml_fault_occurred(&env);

            bool eq = (strcmp(cc, login) == 0);
            free((void*)cc);
            xmlrpc_DECREF(item);
            if (eq)
            {
                xmlrpc_DECREF(cc_member);
                xmlrpc_DECREF(result);
                return true;
            }
        }
        xmlrpc_DECREF(cc_member);
    }

    xmlrpc_DECREF(result);
    return false;
}

void ctx::add_plus_one_cc(uint32_t bug_id, const char* login)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:i,s:{s:(s)}})", "ids", bug_id, "updates", "add_cc", login);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "Bug.update", param, &result);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_DECREF(result);
    xmlrpc_DECREF(param);
}

int32_t ctx::check_uuid_in_bugzilla(const char* component, const char* UUID)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    std::string query = ssprintf("ALL component:\"%s\" statuswhiteboard:\"%s\"", component, UUID);

    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:s})", "quicksearch", query.c_str());
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "Bug.search", param, &result);
    throw_if_xml_fault_occurred(&env);
    xmlrpc_DECREF(param);

    xmlrpc_value* bugs_member = NULL;
    xmlrpc_struct_find_value(&env, result, "bugs", &bugs_member);
    throw_if_xml_fault_occurred(&env);

    if (bugs_member)
    {
        // when array size is equal 0 that means no bug reported
        uint32_t array_size = xmlrpc_array_size(&env, bugs_member);
        throw_if_xml_fault_occurred(&env);
        if (array_size == 0)
        {
            xmlrpc_DECREF(bugs_member);
            xmlrpc_DECREF(result);
            return -1;
        }

        xmlrpc_value* item = NULL;
        xmlrpc_array_read_item(&env, bugs_member, 0, &item); // Correct
        throw_if_xml_fault_occurred(&env);
        xmlrpc_value* bug = NULL;
        xmlrpc_struct_find_value(&env, item, "bug_id", &bug);
        throw_if_xml_fault_occurred(&env);

        if (bug)
        {
            xmlrpc_int bug_id;
            xmlrpc_read_int(&env, bug, &bug_id);
            log("Bug is already reported: %i", (int)bug_id);
            update_client(_("Bug is already reported: %i"), (int)bug_id);

            xmlrpc_DECREF(bug);
            xmlrpc_DECREF(item);
            xmlrpc_DECREF(bugs_member);
            xmlrpc_DECREF(result);
            return bug_id;
        }
        xmlrpc_DECREF(item);
        xmlrpc_DECREF(bugs_member);
    }

    xmlrpc_DECREF(result);
    return -1;
}

uint32_t ctx::new_bug(const map_crash_report_t& pCrashReport)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    std::string package = pCrashReport.find(FILENAME_PACKAGE)->second[CD_CONTENT];
    std::string component = pCrashReport.find(FILENAME_COMPONENT)->second[CD_CONTENT];
    std::string release = pCrashReport.find(FILENAME_RELEASE)->second[CD_CONTENT];
    std::string arch = pCrashReport.find(FILENAME_ARCHITECTURE)->second[CD_CONTENT];
    std::string uuid = pCrashReport.find(CD_UUID)->second[CD_CONTENT];

    std::string summary = "[abrt] crash detected in " + package;
    std::string status_whiteboard = "abrt_hash:" + uuid;

    std::string description;
    create_new_bug_description(pCrashReport, description);

    std::string product;
    std::string version;
    get_product_and_version(release, product, version);

    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:s,s:s,s:s,s:s,s:s,s:s,s:s})",
                                        "product", product.c_str(),
                                        "component", component.c_str(),
                                        "version", version.c_str(),
                                        "summary", summary.c_str(),
                                        "description", description.c_str(),
                                        "status_whiteboard", status_whiteboard.c_str(),
                                        "platform", arch.c_str()
                              );
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result;
    xmlrpc_client_call2(&env, client, server_info, "Bug.create", param, &result);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* id;
    xmlrpc_struct_find_value(&env, result, "id", &id);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_int bug_id = -1;
    if (id)
    {
        xmlrpc_read_int(&env, id, &bug_id);
        throw_if_xml_fault_occurred(&env);
        log("New bug id: %i", bug_id);
        update_client(_("New bug id: %i"), bug_id);
    }

    xmlrpc_DECREF(result);
    xmlrpc_DECREF(param);
    xmlrpc_DECREF(id);
    return bug_id;
}

void ctx::add_attachments(const char* bug_id_str, const map_crash_report_t& pCrashReport)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* result = NULL;

    map_crash_report_t::const_iterator it = pCrashReport.begin();
    for (; it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_ATT)
        {
            std::string description = "File: " + it->first;
            const std::string& to_encode = it->second[CD_CONTENT];
            char *encoded64 = encode_base64(to_encode.c_str(), to_encode.length());
            xmlrpc_value* param = xmlrpc_build_value(&env,"(s{s:s,s:s,s:s,s:s})",
                                              bug_id_str,
                                              "description", description.c_str(),
                                              "filename", it->first.c_str(),
                                              "contenttype", "text/plain",
                                              "data", encoded64
                                      );
            free(encoded64);
            throw_if_xml_fault_occurred(&env);

            xmlrpc_client_call2(&env, client, server_info, "bugzilla.addAttachment", param, &result);
            throw_if_xml_fault_occurred(&env);
            xmlrpc_DECREF(result);
            xmlrpc_DECREF(param);
        }
    }
}

} /* namespace */


/*
 * CReporterBugzilla
 */

CReporterBugzilla::CReporterBugzilla() :
    m_bNoSSLVerify(false),
    m_sBugzillaURL("https://bugzilla.redhat.com"),
    m_sBugzillaXMLRPC("https://bugzilla.redhat.com"XML_RPC_SUFFIX)
{}

CReporterBugzilla::~CReporterBugzilla()
{}

std::string CReporterBugzilla::Report(const map_crash_report_t& pCrashReport,
                                      const map_plugin_settings_t& pSettings,
                                      const std::string& pArgs)
{
    int32_t bug_id = -1;
    std::string Login;
    std::string Password;
    std::string BugzillaXMLRPC;
    std::string BugzillaURL;
    bool NoSSLVerify;
    map_plugin_settings_t settings = parse_settings(pSettings);
    /* if parse_settings fails it returns an empty map so we need to use defaults*/
    if(!settings.empty())
    {
        Login = settings["Login"];
        Password = settings["Password"];
        BugzillaXMLRPC = settings["BugzillaXMLRPC"];
        BugzillaURL = settings["BugzillaURL"];
        NoSSLVerify = settings["NoSSLVerify"] == "yes";
    }
    else
    {
        Login = m_sLogin;
        Password = m_sPassword;
        BugzillaXMLRPC = m_sBugzillaXMLRPC;
        BugzillaURL = m_sBugzillaURL;
        NoSSLVerify = m_bNoSSLVerify;
    }

    std::string component = pCrashReport.find(FILENAME_COMPONENT)->second[CD_CONTENT];
    std::string uuid = pCrashReport.find(CD_UUID)->second[CD_CONTENT];
    try
    {
        ctx bz_server(BugzillaXMLRPC.c_str(), NoSSLVerify);

        update_client(_("Checking for duplicates..."));
        bug_id = bz_server.check_uuid_in_bugzilla(component.c_str(), uuid.c_str());

        update_client(_("Logging into bugzilla..."));
        if ((Login == "") && (Password == ""))
        {
            VERB3 log("Empty login and password");
            throw CABRTException(EXCEP_PLUGIN, std::string(_("Empty login and password. Please check Bugzilla.conf")));
        }
        bz_server.login(Login.c_str(), Password.c_str());

        if (bug_id > 0)
        {
            update_client(_("Checking CC..."));
            if (!bz_server.check_cc_and_reporter(bug_id, Login.c_str()))
            {
                bz_server.add_plus_one_cc(bug_id, Login.c_str());
            }
            bz_server.logout();
            return BugzillaURL + "/show_bug.cgi?id=" + to_string(bug_id);
        }

        update_client(_("Creating new bug..."));
        bug_id = bz_server.new_bug(pCrashReport);
        bz_server.add_attachments(to_string(bug_id).c_str(), pCrashReport);

        update_client(_("Logging out..."));
        bz_server.logout();
    }
    catch (CABRTException& e)
    {
        throw CABRTException(EXCEP_PLUGIN, e.what());
    }

    if (bug_id > 0)
    {
        return BugzillaURL + "/show_bug.cgi?id=" + to_string(bug_id);
    }

    return BugzillaURL + "/show_bug.cgi?id=";
}

map_plugin_settings_t CReporterBugzilla::parse_settings(const map_plugin_settings_t& pSettings)
{
    map_plugin_settings_t plugin_settings;
    map_plugin_settings_t::const_iterator it;
    map_plugin_settings_t::const_iterator end = pSettings.end();

    it = pSettings.find("BugzillaURL");
    if (it != end)
    {
        std::string BugzillaURL = it->second;
        //remove the /xmlrpc.cgi part from old settings
        //FIXME: can be removed after users are informed about new config format
        std::string::size_type pos = BugzillaURL.find(XML_RPC_SUFFIX);
        if (pos != std::string::npos)
        {
            BugzillaURL.erase(pos);
        }
        //remove the trailing '/'
        while (BugzillaURL[BugzillaURL.length() - 1] == '/')
        {
            BugzillaURL.erase(BugzillaURL.length() - 1);
        }
        plugin_settings["BugzillaXMLRPC"] = BugzillaURL + XML_RPC_SUFFIX;
        plugin_settings["BugzillaURL"] = BugzillaURL;
    }

    it = pSettings.find("Login");
    if (it == end)
    {
        /* if any of the option is not set we use the defaults for everything */
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["Login"] = it->second;

    it = pSettings.find("Password");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["Password"] = it->second;

    it = pSettings.find("NoSSLVerify");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["NoSSLVerify"] = it->second;

    VERB1 log("User settings ok, using them instead of defaults");
    return plugin_settings;
}

void CReporterBugzilla::SetSettings(const map_plugin_settings_t& pSettings)
{
//BUG! This gets called when user's keyring contains login data,
//then it takes precedence over /etc/abrt/plugins/Bugzilla.conf.
//I got a case when keyring had a STALE password, and there was no way
//for me to know that it is being used. Moreover, when I discovered it
//(by hacking abrt source!), I don't know how to purge it from the keyring.
//At the very least, log("SOMETHING") here.

    map_plugin_settings_t::const_iterator it;
    map_plugin_settings_t::const_iterator end = pSettings.end();

    it = pSettings.find("BugzillaURL");
    if (it != end)
    {
        m_sBugzillaURL = it->second;
        //remove the /xmlrpc.cgi part from old settings
        //FIXME: can be removed after users are informed about new config format
        std::string::size_type pos = m_sBugzillaURL.find(XML_RPC_SUFFIX);
        if (pos != std::string::npos)
        {
            m_sBugzillaURL.erase(pos);
        }
        //remove the trailing '/'
        while (m_sBugzillaURL[m_sBugzillaURL.length() - 1] == '/')
        {
            m_sBugzillaURL.erase(m_sBugzillaURL.length() - 1);
        }
        /*
        if(*(--m_sBugzillaURL.end()) == '/')
        {
            m_sBugzillaURL.erase(--m_sBugzillaURL.end());
        }
        */
        m_sBugzillaXMLRPC = m_sBugzillaURL + XML_RPC_SUFFIX;
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
        m_bNoSSLVerify = (it->second == "yes");
    }
}

const map_plugin_settings_t& CReporterBugzilla::GetSettings()
{
    m_pSettings["BugzillaURL"] = m_sBugzillaURL;
    m_pSettings["Login"] = m_sLogin;
    m_pSettings["Password"] = m_sPassword;
    m_pSettings["NoSSLVerify"] = m_bNoSSLVerify ? "yes" : "no";

    return m_pSettings;
}

PLUGIN_INFO(REPORTER,
            CReporterBugzilla,
            "Bugzilla",
            "0.0.4",
            "Check if a bug isn't already reported in a bugzilla "
            "and if not, report it.",
            "npajkovs@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Bugzilla.GTKBuilder");
