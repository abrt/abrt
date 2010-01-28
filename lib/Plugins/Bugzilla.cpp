#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include "abrtlib.h"
#include "abrt_xmlrpc.h"
#include "Bugzilla.h"
#include "CrashTypes.h"
#include "DebugDump.h"
#include "ABRTException.h"
#include "CommLayerInner.h"
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define XML_RPC_SUFFIX      "/xmlrpc.cgi"
#define NEW_BUG             -1
#define MISSING_MEMBER      -2

/*
 *  TODO: npajkovs: better deallocation of xmlrpc value
 */

/*
 * Static namespace for xmlrpc stuff.
 * Used mainly to ensure we always destroy xmlrpc client and server_info.
 */

namespace {

struct ctx: public abrt_xmlrpc_conn {
    ctx(const char* url, bool no_ssl_verify): abrt_xmlrpc_conn(url, no_ssl_verify) {}

    void login(const char* login, const char* passwd);
    void logout();
    int32_t check_uuid_in_bugzilla(const char* component, const char* UUID);
    bool check_cc_and_reporter(uint32_t bug_id, const char* login);
    void add_plus_one_cc(uint32_t bug_id, const char* login);
    uint32_t new_bug(const map_crash_data_t& pCrashData);
    void add_attachments(const char* bug_id_str, const map_crash_data_t& pCrashData);
};

void ctx::login(const char* login, const char* passwd)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:s,s:s})", "login", login, "password", passwd);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "User.login", param, &result);
    xmlrpc_DECREF(param);
    if (result)
        xmlrpc_DECREF(result);

    if (env.fault_occurred)
    {
        std::string errmsg = ssprintf("Can't login. Check Edit->Plugins->Bugzilla and /etc/abrt/plugins/Bugzilla.conf. Server said: %s", env.fault_string);
        xmlrpc_env_clean(&env);
        error_msg("%s", errmsg.c_str()); // show error in daemon log
        throw CABRTException(EXCEP_PLUGIN, errmsg.c_str());
    }
}

void ctx::logout()
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "(s)", "");
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "User.logout", param, &result);
    xmlrpc_DECREF(param);
    xmlrpc_DECREF(result);
    throw_if_xml_fault_occurred(&env);
}

bool ctx::check_cc_and_reporter(uint32_t bug_id, const char* login)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "(s)", to_string(bug_id).c_str());
    throw_if_xml_fault_occurred(&env); // failed to allocate memory

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "bugzilla.getBug", param, &result);
    xmlrpc_DECREF(param);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* reporter_member = NULL;
    xmlrpc_struct_find_value(&env, result, "reporter", &reporter_member);
    if (env.fault_occurred)
    {
        xmlrpc_DECREF(result);
        throw_xml_fault(&env);
    }

    if (reporter_member)
    {
        const char* reporter = NULL;
        xmlrpc_read_string(&env, reporter_member, &reporter);
        if (env.fault_occurred)
        {
            xmlrpc_DECREF(result);
            xmlrpc_DECREF(reporter_member);
            throw_xml_fault(&env);
        }

        bool eq = (strcmp(reporter, login) == 0);
        free((void*)reporter);
        xmlrpc_DECREF(reporter_member);
        if (eq)
        {
            xmlrpc_DECREF(result);
            return true;
        }
    }
    else
    {
        VERB3 log("Missing member 'reporter'");
        xmlrpc_DECREF(result);
        throw CABRTException(EXCEP_PLUGIN, _("Missing member 'reporter'"));
    }

    xmlrpc_value* cc_member = NULL;
    xmlrpc_struct_find_value(&env, result, "cc", &cc_member);
    if (env.fault_occurred)
    {
        xmlrpc_DECREF(result);
        throw_xml_fault(&env);
    }

    if (cc_member)
    {
        uint32_t array_size = xmlrpc_array_size(&env, cc_member);

        for (uint32_t i = 0; i < array_size; i++)
        {
            xmlrpc_value* item = NULL;
            xmlrpc_array_read_item(&env, cc_member, i, &item); // Correct
            if (env.fault_occurred)
            {
                xmlrpc_DECREF(result);
                xmlrpc_DECREF(cc_member);
                throw_xml_fault(&env);
            }

            const char* cc = NULL;
            xmlrpc_read_string(&env, item, &cc);
            if (env.fault_occurred)
            {
                xmlrpc_DECREF(result);
                xmlrpc_DECREF(cc_member);
                xmlrpc_DECREF(item);
                throw_xml_fault(&env);
            }

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
    else
    {
        VERB3 log("Missing member 'cc'");
        xmlrpc_DECREF(result);
        throw CABRTException(EXCEP_PLUGIN, _("Missing member 'cc'"));
    }

    xmlrpc_DECREF(result);
    return false;
}

void ctx::add_plus_one_cc(uint32_t bug_id, const char* login)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:i,s:{s:(s)}})", "ids", bug_id, "updates", "add_cc", login); 
    throw_if_xml_fault_occurred(&env); // failed to allocate memory

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "Bug.update", param, &result);
    xmlrpc_DECREF(param);
    xmlrpc_DECREF(result);
    throw_if_xml_fault_occurred(&env);
}

int32_t ctx::check_uuid_in_bugzilla(const char* component, const char* UUID)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    std::string query = ssprintf("ALL component:\"%s\" statuswhiteboard:\"%s\"", component, UUID);

    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:s})", "quicksearch", query.c_str());
    throw_if_xml_fault_occurred(&env); // failed to allocate memory

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "Bug.search", param, &result);
    xmlrpc_DECREF(param);
    throw_if_xml_fault_occurred(&env); // on error result is NULL, no need to DECREF

    xmlrpc_value* bugs_member = NULL;
    xmlrpc_struct_find_value(&env, result, "bugs", &bugs_member);
    if (env.fault_occurred)
    {
        xmlrpc_DECREF(result);
        throw_xml_fault(&env);
    }

    if (bugs_member)
    {
        // when array size is 0 that means no bug reported
        uint32_t array_size = xmlrpc_array_size(&env, bugs_member);
        if (env.fault_occurred)
        {
            xmlrpc_DECREF(result);
            xmlrpc_DECREF(bugs_member);
            throw_xml_fault(&env);
        }
        else if (array_size == 0)
        {
            xmlrpc_DECREF(result);
            xmlrpc_DECREF(bugs_member);
            return NEW_BUG;
        }

        xmlrpc_value* item = NULL;
        xmlrpc_array_read_item(&env, bugs_member, 0, &item); // Correct
        if (env.fault_occurred)
        {
            xmlrpc_DECREF(result);
            xmlrpc_DECREF(bugs_member);
            throw_xml_fault(&env);
        }

        xmlrpc_value* bug = NULL;
        xmlrpc_struct_find_value(&env, item, "bug_id", &bug);
        if (env.fault_occurred)
        {
            xmlrpc_DECREF(result);
            xmlrpc_DECREF(bugs_member);
            xmlrpc_DECREF(item);
            throw_xml_fault(&env);
        }

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
        else
        {
            VERB3 log("Missing member 'bug_id'");
            xmlrpc_DECREF(result);
            throw CABRTException(EXCEP_PLUGIN, _("Missing member 'bug_id'"));
        }
        xmlrpc_DECREF(item);
        xmlrpc_DECREF(bugs_member);
    }
    else
    {
        VERB3 log("Missing member 'bugs'");
        xmlrpc_DECREF(result);
        throw CABRTException(EXCEP_PLUGIN, _("Missing member 'bugs'"));
    }

    xmlrpc_DECREF(result);
    return MISSING_MEMBER;
}

uint32_t ctx::new_bug(const map_crash_data_t& pCrashData)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    const std::string& package   = get_crash_data_item_content(pCrashData, FILENAME_PACKAGE);
    const std::string& component = get_crash_data_item_content(pCrashData, FILENAME_COMPONENT);
    const std::string& release   = get_crash_data_item_content(pCrashData, FILENAME_RELEASE);
    const std::string& arch      = get_crash_data_item_content(pCrashData, FILENAME_ARCHITECTURE);
    const std::string& uuid      = get_crash_data_item_content(pCrashData, CD_DUPHASH);

    std::string summary = "[abrt] crash in " + package;
    std::string status_whiteboard = "abrt_hash:" + uuid;

    std::string description = "abrt "VERSION" detected a crash.\n\n";
    description += make_description_bz(pCrashData);

    std::string product;
    std::string version;
    parse_release(release.c_str(), product, version);

    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:s,s:s,s:s,s:s,s:s,s:s,s:s})",
                                        "product", product.c_str(),
                                        "component", component.c_str(),
                                        "version", version.c_str(),
                                        "summary", summary.c_str(),
                                        "description", description.c_str(),
                                        "status_whiteboard", status_whiteboard.c_str(),
                                        "platform", arch.c_str()
                              );
    throw_if_xml_fault_occurred(&env); // failed to allocate memory

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "Bug.create", param, &result);
    xmlrpc_DECREF(param);
    throw_if_xml_fault_occurred(&env);

    xmlrpc_value* id = NULL;
    xmlrpc_struct_find_value(&env, result, "id", &id);
    if (env.fault_occurred)
    {
        xmlrpc_DECREF(result);
        throw_xml_fault(&env);
    }

    xmlrpc_int bug_id = -1;
    if (id)
    {
        xmlrpc_read_int(&env, id, &bug_id);
        if (env.fault_occurred)
        {
            xmlrpc_DECREF(result);
            xmlrpc_DECREF(id);
            throw_xml_fault(&env);
        }
        log("New bug id: %i", bug_id);
        update_client(_("New bug id: %i"), bug_id);
    }

    xmlrpc_DECREF(result);
    xmlrpc_DECREF(id);
    return bug_id;
}

void ctx::add_attachments(const char* bug_id_str, const map_crash_data_t& pCrashData)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    map_crash_data_t::const_iterator it = pCrashData.begin();
    for (; it != pCrashData.end(); it++)
    {
        const std::string &itemname = it->first;
        const std::string &type = it->second[CD_TYPE];
        const std::string &content = it->second[CD_CONTENT];

        if (type == CD_TXT
         && (content.length() > CD_TEXT_ATT_SIZE || itemname == FILENAME_BACKTRACE)
        ) {
            char *encoded64 = encode_base64(content.c_str(), content.length());
            xmlrpc_value* param = xmlrpc_build_value(&env, "(s{s:s,s:s,s:s,s:s})",
                                              bug_id_str,
                                              "description", ("File: " + itemname).c_str(),
                                              "filename", itemname.c_str(),
                                              "contenttype", "text/plain",
                                              "data", encoded64
                                      );
            free(encoded64);
            throw_if_xml_fault_occurred(&env); // failed to allocate memory

            xmlrpc_value* result = NULL;
            xmlrpc_client_call2(&env, m_pClient, m_pServer_info, "bugzilla.addAttachment", param, &result);
            xmlrpc_DECREF(param);
            xmlrpc_DECREF(result);
            throw_if_xml_fault_occurred(&env);
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

std::string CReporterBugzilla::Report(const map_crash_data_t& pCrashData,
                                      const map_plugin_settings_t& pSettings,
                                      const char *pArgs)
{
    int32_t bug_id = -1;
    std::string Login;
    std::string Password;
    std::string BugzillaXMLRPC;
    std::string BugzillaURL;
    bool NoSSLVerify;
    map_plugin_settings_t settings = parse_settings(pSettings);
    /* if parse_settings fails it returns an empty map so we need to use defaults */
    if (!settings.empty())
    {
        Login = settings["Login"];
        Password = settings["Password"];
        BugzillaXMLRPC = settings["BugzillaXMLRPC"];
        BugzillaURL = settings["BugzillaURL"];
        NoSSLVerify = string_to_bool(settings["NoSSLVerify"].c_str());
    }
    else
    {
        Login = m_sLogin;
        Password = m_sPassword;
        BugzillaXMLRPC = m_sBugzillaXMLRPC;
        BugzillaURL = m_sBugzillaURL;
        NoSSLVerify = m_bNoSSLVerify;
    }

    const std::string& component = get_crash_data_item_content(pCrashData, FILENAME_COMPONENT);
    const std::string& uuid      = get_crash_data_item_content(pCrashData, CD_DUPHASH);
    try
    {
        ctx bz_server(BugzillaXMLRPC.c_str(), NoSSLVerify);

        update_client(_("Checking for duplicates..."));
        bug_id = bz_server.check_uuid_in_bugzilla(component.c_str(), uuid.c_str());

        if ((Login == "") && (Password == ""))
        {
            VERB3 log("Empty login and password");
            throw CABRTException(EXCEP_PLUGIN, _("Empty login and password. Please check Bugzilla.conf"));
        }

        update_client(_("Logging into bugzilla..."));
        bz_server.login(Login.c_str(), Password.c_str());

        if (bug_id > 0)
        {
            update_client(_("Checking CC..."));
            if (!bz_server.check_cc_and_reporter(bug_id, Login.c_str()))
            {
                bz_server.add_plus_one_cc(bug_id, Login.c_str());
            }
            bz_server.logout();
            BugzillaURL += "/show_bug.cgi?id=";
            BugzillaURL += to_string(bug_id);
            return BugzillaURL;
        }

        update_client(_("Creating new bug..."));
        bug_id = bz_server.new_bug(pCrashData);
        bz_server.add_attachments(to_string(bug_id).c_str(), pCrashData);

        update_client(_("Logging out..."));
        bz_server.logout();
    }
    catch (CABRTException& e)
    {
        throw CABRTException(EXCEP_PLUGIN, e.what());
    }

    if (bug_id > 0)
    {
        BugzillaURL += "/show_bug.cgi?id=";
        BugzillaURL += to_string(bug_id);
        return BugzillaURL;
    }

    BugzillaURL += "/show_bug.cgi?id=";
    return BugzillaURL;
}

//todo: make static
map_plugin_settings_t CReporterBugzilla::parse_settings(const map_plugin_settings_t& pSettings)
{
    map_plugin_settings_t plugin_settings;

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
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
    m_pSettings = pSettings;

//BUG! This gets called when user's keyring contains login data,
//then it takes precedence over /etc/abrt/plugins/Bugzilla.conf.
//I got a case when keyring had a STALE password, and there was no way
//for me to know that it is being used. Moreover, when I discovered it
//(by hacking abrt source!), I don't know how to purge it from the keyring.
//At the very least, log("SOMETHING") here.

    map_plugin_settings_t::const_iterator end = pSettings.end();
    map_plugin_settings_t::const_iterator it;
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
        if (*(--m_sBugzillaURL.end()) == '/')
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
        m_bNoSSLVerify = string_to_bool(it->second.c_str());
    }
}

/* Should not be deleted (why?) */
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
            "Reports bugs to bugzilla",
            "npajkovs@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Bugzilla.GTKBuilder");
