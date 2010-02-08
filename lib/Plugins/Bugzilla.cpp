/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
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
#define MAX_HOPPS            5

static int32_t abrt_errno;

typedef enum {
    ABRTE_BUGZILLA_OK = 0,
    ABRTE_BUGZILLA_NEW_BUG,                             /* 1 */
    ABRTE_BUGZILLA_NO_DATA,                             /* 2 */
    ABRTE_BUGZILLA_XMLRPC_ERROR,                        /* 3 */
    ABRTE_BUGZILLA_XMLRPC_MISSING_MEMBER                /* 4 */
}bugzilla_codes;


static const char* abrt_strerror(bugzilla_codes code)
{
    return NULL;
}

/*
 *  TODO: npajkovs: better deallocation of xmlrpc value
 *        npajkovs: better gathering function which collects all information from bugzilla
 *        npajkovs: figure out how to deal with cloning bugs
 *        npajkovs: check if attachment was uploaded successul an if not try it again(max 3 times)
 *                  and if it still fails. retrun successful, but mention that attaching failed
 *        npajkovs: add new fce to add comments
 */

struct bug_info {
    const char* bug_status;
    const char* bug_resolution;
    const char* bug_reporter;
    uint32_t bug_dup_id;
    std::vector<const char*> bug_cc;
};

static void bug_info_init(struct bug_info* bz);
static void bug_info_destroy(struct bug_info* bz);

static int32_t am_i_in_cc(const struct bug_info* bz, const char* login);

static void bug_info_init(struct bug_info* bz)
{
    bz->bug_status = NULL;
    bz->bug_resolution = NULL;
    bz->bug_reporter = NULL;
}

static void bug_info_destroy(struct bug_info* bz)
{
    if (bz->bug_status != NULL)
        free((void*)bz->bug_status);

    if (bz->bug_resolution != NULL)
        free((void*)bz->bug_resolution);

    if (bz->bug_reporter != NULL)
        free((void*)bz->bug_reporter);

    if (!bz->bug_cc.empty())
    {
        for( int32_t ii = 0; ii < bz->bug_cc.size(); ii++)
            free((void*)bz->bug_cc[ii]);

        bz->bug_cc.clear();
    }
}

static int32_t am_i_in_cc(const struct bug_info* bz, const char* login)
{
    if (bz->bug_cc.empty())
        return -1;

    int32_t size = bz->bug_cc.size();
    for (int32_t ii = 0; ii < size; ii++)
    {
        if (strcmp(login, bz->bug_cc[ii]) == 0)
            return 0;
    }
    return -1;
}

/*
 * Static namespace for xmlrpc stuff.
 * Used mainly to ensure we always destroy xmlrpc client and server_info.
 */

namespace {

struct ctx: public abrt_xmlrpc_conn {
    ctx(const char* url, bool no_ssl_verify): abrt_xmlrpc_conn(url, no_ssl_verify) {}

    bool check_cc_and_reporter(uint32_t bug_id, const char* login);
    void login(const char* login, const char* passwd);
    void logout();

    const char* get_bug_status(xmlrpc_env* env, xmlrpc_value* result_xml);
    const char* get_bug_resolution(xmlrpc_env* env, xmlrpc_value* result_xml);
    const char* get_bug_reporter(xmlrpc_env* env, xmlrpc_value* result_xml);

    xmlrpc_value* call_quicksearch_uuid(xmlrpc_env* env, const char* component, const char* uuid);
    xmlrpc_value* get_cc_member(xmlrpc_env* env, xmlrpc_value* result_xml);
    xmlrpc_value* get_member(xmlrpc_env* env, const char* member, xmlrpc_value* result_xml);

    int32_t get_array_size(xmlrpc_env* env, xmlrpc_value* result_xml);
    int32_t get_bug_id(xmlrpc_env* env, xmlrpc_value* result_xml);
    int32_t get_bug_dup_id(xmlrpc_env* env, xmlrpc_value* result_xml);
    int32_t get_bug_cc(xmlrpc_env* env, xmlrpc_value* result_xml, struct bug_info* bz);
    int32_t add_plus_one_cc(xmlrpc_env* env, uint32_t bug_id, const char* login);
    int32_t new_bug(xmlrpc_env* env, const map_crash_data_t& pCrashData);
    int32_t add_attachments(xmlrpc_env* env, const char* bug_id_str, const map_crash_data_t& pCrashData);
    int32_t get_bug_info(xmlrpc_env* env, struct bug_info* bz, uint32_t bug_id);
    int32_t add_comment(xmlrpc_env* env, uint32_t bug_id, const char* comment);
};

xmlrpc_value* ctx::get_member(xmlrpc_env* env, const char* member, xmlrpc_value* result_xml)
{
    xmlrpc_value* cc_member = NULL;
    xmlrpc_struct_find_value(env, result_xml, member, &cc_member);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return NULL;
    }

    if (cc_member)
        return cc_member;

    abrt_errno = ABRTE_BUGZILLA_XMLRPC_MISSING_MEMBER;
    return NULL;
}

int32_t ctx::get_array_size(xmlrpc_env* env, xmlrpc_value* result_xml)
{
    // The only way this can fail is if 'bugs_member' is not actually an array XML-RPC value. So it is usually not worth checking 'env'.
    int32_t size = xmlrpc_array_size(env, result_xml);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }
    return size;
}

int32_t ctx::get_bug_dup_id(xmlrpc_env* env, xmlrpc_value* result_xml)
{
    xmlrpc_value* dup_id = get_member(env, "dup_id", result_xml);
    if (!dup_id)
        return -1;

    xmlrpc_int dup_id_int = -1;
    xmlrpc_read_int(env, dup_id, &dup_id_int);
    xmlrpc_DECREF(dup_id);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }
    VERB3 log("get dup_id: %i", dup_id_int);
    abrt_errno = ABRTE_BUGZILLA_OK;
    return dup_id_int;
}

const char* ctx::get_bug_reporter(xmlrpc_env* env, xmlrpc_value* result_xml)
{
    xmlrpc_value* reporter_member = get_member(env, "reporter", result_xml);
    if (!reporter_member)
        return NULL;

    const char* reporter = NULL;
    xmlrpc_read_string(env, reporter_member, &reporter);
    xmlrpc_DECREF(reporter_member);

    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return NULL;
    }

    if (*reporter != '\0')
    {
        VERB3 log("get bug reporter: %s", reporter);
        abrt_errno = ABRTE_BUGZILLA_OK;
        return reporter;
    }
    free((void*)reporter);
    abrt_errno = ABRTE_BUGZILLA_NO_DATA;
    return NULL;
}

const char* ctx::get_bug_resolution(xmlrpc_env* env, xmlrpc_value* result_xml)
{
    xmlrpc_value* bug_resolution = get_member(env, "resolution", result_xml);
    if (!bug_resolution)
        return NULL;

    const char* resolution_str = NULL;
    xmlrpc_read_string(env, bug_resolution, &resolution_str);
    xmlrpc_DECREF(bug_resolution);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return NULL;
    }

    if (*resolution_str != '\0')
    {
        VERB3 log("get resolution: %s", resolution_str);
        abrt_errno = ABRTE_BUGZILLA_OK;
        return resolution_str;
    }
    free((void*)resolution_str);
    abrt_errno = ABRTE_BUGZILLA_NO_DATA;
    return NULL;
}

const char* ctx::get_bug_status(xmlrpc_env* env, xmlrpc_value* result_xml)
{
    xmlrpc_value* bug_status = get_member(env, "bug_status", result_xml);
    if (!bug_status)
        return NULL;

    const char* status_str = NULL;
    xmlrpc_read_string(env, bug_status, &status_str);
    xmlrpc_DECREF(bug_status);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return NULL;
    }

    if (*status_str != '\0')
    {
        VERB3 log("get bug_status: %s", status_str);
        abrt_errno = ABRTE_BUGZILLA_OK;
        return status_str;
    }
    free((void*)status_str);
    abrt_errno = ABRTE_BUGZILLA_NO_DATA;
    return NULL;
}

int32_t ctx::get_bug_cc(xmlrpc_env* env, xmlrpc_value* result_xml, struct bug_info* bz)
{
    xmlrpc_value* cc_member = get_member(env, "cc", result_xml);
    if (!cc_member)
        return -1;

    int32_t array_size = xmlrpc_array_size(env, cc_member);
    if (array_size == -1)
        return -1;

    VERB3 log("count members on cc %i", array_size);

    int32_t real_read = 0;

    for (int32_t i = 0; i < array_size; i++)
    {
        xmlrpc_value* item = NULL;
        xmlrpc_array_read_item(env, cc_member, i, &item);
        if (env->fault_occurred)
        {
            abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
            return -1;
        }

        if (item)
        {
            const char* cc = NULL;
            xmlrpc_read_string(env, item, &cc);
            xmlrpc_DECREF(item);
            if (env->fault_occurred)
            {
                xmlrpc_DECREF(cc_member);
                abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
                return -1;
            }

            if (*cc != '\0')
            {
                bz->bug_cc.push_back(cc);
                VERB3 log("member on cc is %s", cc);
                ++real_read;
                continue;
            }
            free((void*)cc);
        }
    }
    xmlrpc_DECREF(cc_member);
    return real_read;
}

xmlrpc_value* ctx::call_quicksearch_uuid(xmlrpc_env* env, const char* component, const char* uuid)
{
    std::string query = ssprintf("ALL component:\"%s\" statuswhiteboard:\"%s\"", component, uuid);

    // fails only on memory allocation
    xmlrpc_value* param = xmlrpc_build_value(env, "({s:s})", "quicksearch", query.c_str());
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return NULL;
    }

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(env, m_pClient, m_pServer_info, "Bug.search", param, &result);
    xmlrpc_DECREF(param);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return NULL;
    }
    return result;
}

int32_t ctx::get_bug_id(xmlrpc_env* env, xmlrpc_value* result_xml)
{
        xmlrpc_value* item = NULL;
        xmlrpc_array_read_item(env, result_xml, 0, &item);
        if (env->fault_occurred)
        {
            abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
            return -1;
        }

        xmlrpc_value* bug = get_member(env, "bug_id", item);
        xmlrpc_DECREF(item);
        if (!bug)
            return -1;

        xmlrpc_int bug_id = -1;
        xmlrpc_read_int(env, bug, &bug_id);
        xmlrpc_DECREF(bug);
        if (env->fault_occurred)
        {
            abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
            return -1;
        }
        log("Bug is already reported: %i", (int)bug_id);
        update_client(_("Bug is already reported: %i"), (int)bug_id);

        return bug_id;
}

int32_t ctx::add_plus_one_cc(xmlrpc_env* env, uint32_t bug_id, const char* login)
{
    xmlrpc_value* param = xmlrpc_build_value(env, "({s:i,s:{s:(s)}})", "ids", bug_id, "updates", "add_cc", login);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(env, m_pClient, m_pServer_info, "Bug.update", param, &result);
    xmlrpc_DECREF(param);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }

    xmlrpc_DECREF(result);
    return ABRTE_BUGZILLA_OK;
}

int32_t ctx::add_comment(xmlrpc_env* env, uint32_t bug_id, const char* comment)
{
    xmlrpc_value* param = xmlrpc_build_value(env, "({s:i,s:{s:s}})", "ids", bug_id, "updates", "comment", comment);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(env, m_pClient, m_pServer_info, "Bug.update", param, &result);
    xmlrpc_DECREF(param);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }

    return ABRTE_BUGZILLA_OK;
}

int32_t ctx::new_bug(xmlrpc_env* env, const map_crash_data_t& pCrashData)
{

    const std::string& package   = get_crash_data_item_content(pCrashData, FILENAME_PACKAGE);
    const std::string& component = get_crash_data_item_content(pCrashData, FILENAME_COMPONENT);
    const std::string& release   = get_crash_data_item_content(pCrashData, FILENAME_RELEASE);
    const std::string& arch      = get_crash_data_item_content(pCrashData, FILENAME_ARCHITECTURE);
    const std::string& uuid      = get_crash_data_item_content(pCrashData, CD_DUPHASH);
    const char *reason           = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_REASON);

    std::string summary = "[abrt] crash in " + package;
    if (reason != NULL)
    {
        summary += ": ";
        summary += reason;
    }
    std::string status_whiteboard = "abrt_hash:" + uuid;

    std::string description = "abrt "VERSION" detected a crash.\n\n";
    description += make_description_bz(pCrashData);

    std::string product;
    std::string version;
    parse_release(release.c_str(), product, version);

    xmlrpc_value* param = xmlrpc_build_value(env, "({s:s,s:s,s:s,s:s,s:s,s:s,s:s})",
                                        "product", product.c_str(),
                                        "component", component.c_str(),
                                        "version", version.c_str(),
                                        "summary", summary.c_str(),
                                        "description", description.c_str(),
                                        "status_whiteboard", status_whiteboard.c_str(),
                                        "platform", arch.c_str()
                              );
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(env, m_pClient, m_pServer_info, "Bug.create", param, &result);
    xmlrpc_DECREF(param);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }

    xmlrpc_value* id = get_member(env, "id", result);
    xmlrpc_DECREF(result);
    if (!id)
        return -1;

    xmlrpc_int bug_id = -1;
    xmlrpc_read_int(env, id, &bug_id);
    if (env->fault_occurred)
    {
        xmlrpc_DECREF(id);
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }
    log("New bug id: %i", bug_id);
    update_client(_("New bug id: %i"), bug_id);

    xmlrpc_DECREF(id);
    return bug_id;
}

int32_t ctx::add_attachments(xmlrpc_env* env, const char* bug_id_str, const map_crash_data_t& pCrashData)
{
    xmlrpc_value* result = NULL;

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
            xmlrpc_value* param = xmlrpc_build_value(env, "(s{s:s,s:s,s:s,s:s})",
                                              bug_id_str,
                                              "description", ("File: " + itemname).c_str(),
                                              "filename", itemname.c_str(),
                                              "contenttype", "text/plain",
                                              "data", encoded64
                                      );
            free(encoded64);
            if (env->fault_occurred)
            {
                abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
                return -1;
            }

            xmlrpc_client_call2(env, m_pClient, m_pServer_info, "bugzilla.addAttachment", param, &result);
            xmlrpc_DECREF(param);
            if (result)
                xmlrpc_DECREF(result);

            if (env->fault_occurred)
            {
                abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
                return -1;
            }
        }
    }
    return ABRTE_BUGZILLA_OK;
}

int32_t ctx::get_bug_info(xmlrpc_env* env, struct bug_info* bz, uint32_t bug_id)
{
    xmlrpc_value* param = xmlrpc_build_value(env, "(s)", to_string(bug_id).c_str());
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }

    xmlrpc_value* result = NULL;
    xmlrpc_client_call2(env, m_pClient, m_pServer_info, "bugzilla.getBug", param, &result);
    xmlrpc_DECREF(param);
    if (env->fault_occurred)
    {
        abrt_errno = ABRTE_BUGZILLA_XMLRPC_ERROR;
        return -1;
    }

    if (result)
    {
        // mandatory
        bz->bug_status = get_bug_status(env, result);
        if (bz->bug_status == NULL)
            return -1;

        // mandatory when bug status is CLOSED
        bz->bug_resolution = get_bug_resolution(env, result);
        if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
            return -1;

        bz->bug_dup_id = get_bug_dup_id(env, result);
        if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
            return -1;

        bz->bug_reporter = get_bug_reporter(env, result);
        if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
            return -1;

        get_bug_cc(env, result, bz);
        if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
            return -1;
        xmlrpc_DECREF(result);
     }
}

//-------------------------------------------------------------------
//                           ^
//                           |  nice
// -------------------------------------------------------------------
//                           |  BAD
//                           v
//-------------------------------------------------------------------
//TODO: need to rewrite
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
    if (result)
        xmlrpc_DECREF(result);

    throw_if_xml_fault_occurred(&env);
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

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    ctx bz_server(BugzillaXMLRPC.c_str(), NoSSLVerify);

    update_client(_("Logging into bugzilla..."));
    if ((Login == "") && (Password == ""))
    {
        VERB3 log("Empty login and password");
        throw CABRTException(EXCEP_PLUGIN, _("Empty login and password. Please check Bugzilla.conf"));
    }

    bz_server.login(Login.c_str(), Password.c_str());
    update_client(_("Checking for duplicates..."));

    xmlrpc_value* result = bz_server.call_quicksearch_uuid(&env, component.c_str(), uuid.c_str());
    if (!result)
    {
        if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
            throw_if_xml_fault_occurred(&env);
        else
        {
                // TODO: check ours returned value
        }
    }

    xmlrpc_value* all_bugs = bz_server.get_member(&env, "bugs", result);
    xmlrpc_DECREF(result);

    if (!all_bugs)
    {
        if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
            throw_if_xml_fault_occurred(&env);
        else
        {
            // TODO: check ours returned value
        }
    }

    int32_t all_bugs_size = bz_server.get_array_size(&env, all_bugs);

    if (all_bugs_size == -1)
    {
        if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
            throw_if_xml_fault_occurred(&env);
        else
        {
            // TODO: check ours returned value
        }
    }
    else if (all_bugs_size == 0) // Create new bug
    {
        update_client(_("Creating new bug..."));
        bug_id = bz_server.new_bug(&env, pCrashData);
        int32_t ret = bz_server.add_attachments(&env, to_string(bug_id).c_str(), pCrashData);

        update_client(_("Logging out..."));
        bz_server.logout();

        BugzillaURL += "/show_bug.cgi?id=";
        BugzillaURL += to_string(bug_id);
        return BugzillaURL;
    }
    else if (all_bugs_size > 1)
    {
        // When someone clones bug it has same uuid, so we can find more then 1. Need to be checked if component is same.
        VERB3 log("Bugzilla has %i same uuids(%s)", all_bugs_size, uuid.c_str());
    }

    // desicition based on state
    bug_id = bz_server.get_bug_id(&env, all_bugs);
    xmlrpc_DECREF(all_bugs);
    if (bug_id == -1)
    {
        if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
            throw_if_xml_fault_occurred(&env);
        else
        {
            // TODO: check ours returned value
        }

    }

    struct bug_info bz;
    bug_info_init(&bz);
    if (bz_server.get_bug_info(&env, &bz, bug_id) == -1)
    {
        if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
            throw_if_xml_fault_occurred(&env);
        else
        {
            // TODO: check ours returned value
        }
    }

    int32_t original_bug_id = bug_id;
    if ((strcmp(bz.bug_status, "CLOSED") == 0) && (strcmp(bz.bug_resolution, "DUPLICATE") == 0))
    {
        for (int32_t ii = 0; ii <= MAX_HOPPS; ii++)
        {
            if (ii == MAX_HOPPS)
            {
                VERB3 log("Bugzilla couldn't find parent of bug(%d)", original_bug_id);
                bug_info_destroy(&bz);
                throw CABRTException(EXCEP_PLUGIN, _("Bugzilla couldn't find parent of bug(%d)"), original_bug_id);
            }

            VERB3 log("Bugzilla(%d): Jump to bug %d", bug_id, bz.bug_dup_id);
            bug_id = bz.bug_dup_id;
            update_client(_("Jump to bug %d"), bug_id);
            bug_info_destroy(&bz);
            bug_info_init(&bz);

            if (bz_server.get_bug_info(&env, &bz, bug_id) == -1)
            {
                if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
                {
                    bug_info_destroy(&bz);
                    throw_if_xml_fault_occurred(&env);
                }
                else
                {
                    // TODO: check ours returned value
                }
            }

            // found a bug which is not CLOSED as DUPLICATE
            if (bz.bug_dup_id == -1)
                break;
        }
    }

    if (strcmp(bz.bug_status, "CLOSED") != 0)
    {
        int32_t status = 0;
        if ((strcmp(bz.bug_reporter, Login.c_str()) != 0) && (am_i_in_cc(&bz, Login.c_str())))
        {
            VERB2 log(_("Add %s to CC list"), Login.c_str());
            update_client(_("Add %s to CC list"), Login.c_str());
            status = bz_server.add_plus_one_cc(&env, bug_id, Login.c_str());
        }

        bug_info_destroy(&bz);
        if (status == -1)
        {
            if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
                throw_if_xml_fault_occurred(&env);
            else
            {
                // TODO: check ours returned value
            }

        }

        std::string description = make_description_reproduce_comment(pCrashData);
        if (!description.empty())
        {
            VERB3 log("Add new comment into bug(%d)", bug_id);
            update_client(_("Add new comment into bug(%d)"),bug_id);
            if (bz_server.add_comment(&env, bug_id, description.c_str()) == -1)
            {
                if (abrt_errno == ABRTE_BUGZILLA_XMLRPC_ERROR)
                    throw_if_xml_fault_occurred(&env);
                else
                {
                    // TODO: check ours returned value
                }
            }
        }

        update_client(_("Logging out..."));
        bz_server.logout();
        BugzillaURL += "/show_bug.cgi?id=";
        BugzillaURL += to_string(bug_id);
        return BugzillaURL;
    }

    update_client(_("Logging out..."));
    bz_server.logout();

    BugzillaURL += "/show_bug.cgi?id=";
    BugzillaURL += to_string(bug_id);
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
