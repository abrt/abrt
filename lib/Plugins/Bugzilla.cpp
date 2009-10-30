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

static xmlrpc_env env;
static xmlrpc_client* client = NULL;
static struct xmlrpc_clientparms clientParms;
static struct xmlrpc_curl_xportparms curlParms;
static xmlrpc_server_info* server_info = NULL;


static void login(const char* login, const char* passwd);

static void logout();

static void new_xmlrpc_client(const char* url, bool no_ssl_verify);

static void destroy_xmlrpc_client();

static int32_t check_uuid_in_bugzilla(const char* component, const char* UUID);

static bool check_cc_and_reporter(const uint32_t bug_id, const char* login);

static void add_plus_one_cc(const uint32_t bug_id, const char* login);

static void create_new_bug_description(const map_crash_report_t& pCrashReport, std::string& pDescription);

static void get_product_and_version(const std::string& pRelease,
                                          std::string& pProduct,
                                          std::string& pVersion);


// FIXME: we still leak memmory if this function detects a fault:
// many instances when we leave non-freed or non-xmlrpc_DECREF'ed data behind.
static void throw_if_xml_fault_occurred()
{
    if (env.fault_occurred)
    {
        std::string errmsg = ssprintf("XML-RPC Fault: %s(%d)", env.fault_string, env.fault_code);
        error_msg("%s", errmsg.c_str()); // show error in daemon log
        throw CABRTException(EXCEP_PLUGIN, errmsg);
    }
}

static void new_xmlrpc_client(const char* url, bool no_ssl_verify)
{
    xmlrpc_env_init(&env);
    xmlrpc_client_setup_global_const(&env);

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
    xmlrpc_client_teardown_global_const();
}

CReporterBugzilla::CReporterBugzilla() :
    m_bNoSSLVerify(false),
    m_sBugzillaURL("https://bugzilla.redhat.com"),
    m_sBugzillaXMLRPC("https://bugzilla.redhat.com"XML_RPC_SUFFIX)
{}

CReporterBugzilla::~CReporterBugzilla()
{}

static void login(const char* login, const char* passwd)
{
    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:s,s:s})", "login", login, "password", passwd);
    throw_if_xml_fault_occurred();

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "User.login", param, &result);
    throw_if_xml_fault_occurred();

    xmlrpc_DECREF(result);
    xmlrpc_DECREF(param);
}

static void logout()
{
    xmlrpc_value* param = xmlrpc_build_value(&env, "(s)", "");
    throw_if_xml_fault_occurred();

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "User.logout", param, &result);
    throw_if_xml_fault_occurred();

    xmlrpc_DECREF(result);
    xmlrpc_DECREF(param);
}

static bool check_cc_and_reporter(const uint32_t bug_id, const char* login)
{
    xmlrpc_value* param = xmlrpc_build_value(&env, "(s)", to_string(bug_id).c_str());
    throw_if_xml_fault_occurred();

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "bugzilla.getBug", param, &result);
    throw_if_xml_fault_occurred();
    xmlrpc_DECREF(param);

    xmlrpc_value* reporter_member = NULL;
    xmlrpc_struct_find_value(&env, result, "reporter", &reporter_member);
    throw_if_xml_fault_occurred();

    if (reporter_member)
    {
        const char* reporter = NULL;
        xmlrpc_read_string(&env, reporter_member, &reporter);
        throw_if_xml_fault_occurred();

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
    throw_if_xml_fault_occurred();

    if (cc_member)
    {
        uint32_t array_size = xmlrpc_array_size(&env, cc_member);

        for (uint32_t i = 0; i < array_size; i++)
        {
            xmlrpc_value* item = NULL;
            xmlrpc_array_read_item(&env, cc_member, i, &item); // Correct
            throw_if_xml_fault_occurred();

            const char* cc = NULL;
            xmlrpc_read_string(&env, item, &cc);
            throw_if_xml_fault_occurred();

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

static void add_plus_one_cc(const uint32_t bug_id, const char* login)
{
    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:i,s:{s:(s)}})", "ids", bug_id, "updates", "add_cc", login);
    throw_if_xml_fault_occurred();

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "Bug.update", param, &result);
    throw_if_xml_fault_occurred();

    xmlrpc_DECREF(result);
    xmlrpc_DECREF(param);
}

static int32_t check_uuid_in_bugzilla(const char* component, const char* UUID)
{
    std::string query = ssprintf("ALL component:\"%s\" statuswhiteboard:\"%s\"", component, UUID);

    xmlrpc_value* param = xmlrpc_build_value(&env, "({s:s})", "quicksearch", query.c_str());
    throw_if_xml_fault_occurred();

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(&env, client, server_info, "Bug.search", param, &result);
    throw_if_xml_fault_occurred();
    xmlrpc_DECREF(param);

    xmlrpc_value* bugs_member = NULL;
    xmlrpc_struct_find_value(&env, result, "bugs", &bugs_member);
    throw_if_xml_fault_occurred();

    if (bugs_member)
    {
        // when array size is equal 0 that means no bug reported
        uint32_t array_size = xmlrpc_array_size(&env, bugs_member);
        throw_if_xml_fault_occurred();
        if (array_size == 0)
        {
            xmlrpc_DECREF(bugs_member);
            xmlrpc_DECREF(result);
            return -1;
        }

        xmlrpc_value* item = NULL;
        xmlrpc_array_read_item(&env, bugs_member, 0, &item); // Correct
        throw_if_xml_fault_occurred();
        xmlrpc_value* bug = NULL;
        xmlrpc_struct_find_value(&env, item, "bug_id", &bug);
        throw_if_xml_fault_occurred();

        if (bug)
        {
            xmlrpc_int bug_id;
            xmlrpc_read_int(&env, bug, &bug_id);
            log("Bug is already reported: %i", (int)bug_id);
            update_client(_("Bug is already reported: ") + to_string(bug_id));

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

static void create_new_bug_description(const map_crash_report_t& pCrashReport, std::string& pDescription)
{
    std::string howToReproduce;
    std::string comment;

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

    map_crash_report_t::const_iterator it = pCrashReport.begin();
    for (; it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] == CD_TXT)
        {
            if (it->first !=  CD_UUID &&
                it->first !=  FILENAME_ARCHITECTURE &&
                it->first !=  FILENAME_RELEASE &&
                it->first !=  CD_REPRODUCE &&
                it->first !=  CD_COMMENT)
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
            std::string msg = ssprintf(_("Binary file %s will not be reported."), it->first.c_str());
            warn_client(msg);
            //update_client(_("Binary file ")+it->first+_(" will not be reported."));
        }
    }
}

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

static uint32_t new_bug(const map_crash_report_t& pCrashReport)
{
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
    throw_if_xml_fault_occurred();

    xmlrpc_value* result;
    xmlrpc_client_call2(&env, client, server_info, "Bug.create", param, &result);
    throw_if_xml_fault_occurred();

    xmlrpc_value* id;
    xmlrpc_struct_find_value(&env, result, "id", &id);
    throw_if_xml_fault_occurred();

    xmlrpc_int bug_id = -1;
    if (id)
    {
        xmlrpc_read_int(&env, id, &bug_id);
        throw_if_xml_fault_occurred();
        log("New bug id: %i", bug_id);
        update_client(_("New bug id: ") + to_string(bug_id));
    }

    xmlrpc_DECREF(result);
    xmlrpc_DECREF(param);
    xmlrpc_DECREF(id);
    return bug_id;
}

static void add_attachments(const std::string& pBugId, const map_crash_report_t& pCrashReport)
{
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
                                              pBugId.c_str(),
                                              "description", description.c_str(),
                                              "filename", it->first.c_str(),
                                              "contenttype", "text/plain",
                                              "data", encoded64
                                      );
            free(encoded64);
            throw_if_xml_fault_occurred();

            xmlrpc_client_call2(&env, client, server_info, "bugzilla.addAttachment", param, &result);
            throw_if_xml_fault_occurred();
            xmlrpc_DECREF(result);
            xmlrpc_DECREF(param);
        }
    }
}

std::string CReporterBugzilla::Report(const map_crash_report_t& pCrashReport, const std::string& pArgs)
{
    int32_t bug_id = -1;

    std::string component = pCrashReport.find(FILENAME_COMPONENT)->second[CD_CONTENT];
    std::string uuid = pCrashReport.find(CD_UUID)->second[CD_CONTENT];
    try
    {
        new_xmlrpc_client(m_sBugzillaXMLRPC.c_str(), m_bNoSSLVerify);

        update_client(_("Checking for duplicates..."));
        bug_id = check_uuid_in_bugzilla(component.c_str(), uuid.c_str());

        update_client(_("Logging into bugzilla..."));
        if ((m_sLogin == "") && (m_sPassword==""))
        {
            VERB3 log("Empty login and password");
            throw CABRTException(EXCEP_PLUGIN, std::string(_("Empty login and password. Please check Bugzilla.conf")));
        }
        login(m_sLogin.c_str(), m_sPassword.c_str());

        if (bug_id > 0)
        {
            update_client(_("Checking CC..."));
            if (!check_cc_and_reporter(bug_id, m_sLogin.c_str()))
            {
                add_plus_one_cc(bug_id, m_sLogin.c_str());
            }
            destroy_xmlrpc_client();
            return m_sBugzillaURL + "/show_bug.cgi?id="+to_string(bug_id);
        }

        update_client(_("Creating new bug..."));
        bug_id = new_bug(pCrashReport);
        add_attachments(to_string(bug_id), pCrashReport);

        update_client(_("Logging out..."));
        logout();

    }
    catch (CABRTException& e)
    {
        destroy_xmlrpc_client();
        throw CABRTException(EXCEP_PLUGIN, std::string("CReporterBugzilla::Report(): ") + e.what());
    }
    destroy_xmlrpc_client();

    if (bug_id > 0)
    {
        return m_sBugzillaURL + "/show_bug.cgi?id="+to_string(bug_id);
    }

    return m_sBugzillaURL + "/show_bug.cgi?id=";
}

void CReporterBugzilla::SetSettings(const map_plugin_settings_t& pSettings)
{
    if (pSettings.find("BugzillaURL") != pSettings.end())
    {
        m_sBugzillaURL = pSettings.find("BugzillaURL")->second;
        //remove the /xmlrpc.cgi part from old settings
        //FIXME: can be removed after users are informed about new config format
        std::string::size_type pos = m_sBugzillaURL.find(XML_RPC_SUFFIX);
        if(pos != std::string::npos)
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
    if (pSettings.find("Login") != pSettings.end())
    {
        m_sLogin = pSettings.find("Login")->second;
    }
    if (pSettings.find("Password") != pSettings.end())
    {
        m_sPassword = pSettings.find("Password")->second;
    }
    if (pSettings.find("NoSSLVerify") != pSettings.end())
    {
        m_bNoSSLVerify = pSettings.find("NoSSLVerify")->second == "yes";
    }
}

map_plugin_settings_t CReporterBugzilla::GetSettings()
{
    map_plugin_settings_t ret;

    ret["BugzillaURL"] = m_sBugzillaURL;
    ret["Login"] = m_sLogin;
    ret["Password"] = m_sPassword;
    ret["NoSSLVerify"] = m_bNoSSLVerify ? "yes" : "no";

    return ret;
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
