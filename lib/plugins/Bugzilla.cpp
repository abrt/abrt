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

#include "abrtlib.h"
#include "abrt_xmlrpc.h"
#include "Bugzilla.h"
#include "crash_types.h"
#include "debug_dump.h"
#include "abrt_exception.h"
#include "comm_layer_inner.h"

#define XML_RPC_SUFFIX      "/xmlrpc.cgi"
#define MAX_HOPS            5


/*
 *  TODO: npajkovs: better deallocation of xmlrpc value
 *        npajkovs: better gathering function which collects all information from bugzilla
 *        npajkovs: figure out how to deal with cloning bugs
 *        npajkovs: check if attachment was uploaded successul an if not try it again(max 3 times)
 *                  and if it still fails. retrun successful, but mention that attaching failed
 *        npajkovs: add option to set comment privat
 */

struct bug_info {
    const char* bug_status;
    const char* bug_resolution;
    const char* bug_reporter;
    const char* bug_product;
    xmlrpc_int32 bug_dup_id;
    std::vector<const char*> bug_cc;
};

static void bug_info_init(struct bug_info* bz)
{
    bz->bug_status = NULL;
    bz->bug_resolution = NULL;
    bz->bug_reporter = NULL;
    bz->bug_product = NULL;
    bz->bug_dup_id = -1;
}

static void bug_info_destroy(struct bug_info* bz)
{
    free((void*)bz->bug_status);
    free((void*)bz->bug_resolution);
    free((void*)bz->bug_reporter);
    free((void*)bz->bug_product);

    if (!bz->bug_cc.empty())
    {
        for (int ii = 0; ii < bz->bug_cc.size(); ii++)
            free((void*)bz->bug_cc[ii]);

        bz->bug_cc.clear();
    }
}

static int am_i_in_cc(const struct bug_info* bz, const char* login)
{
    if (bz->bug_cc.empty())
        return -1;

    int size = bz->bug_cc.size();
    for (int ii = 0; ii < size; ii++)
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
    xmlrpc_env env;

    ctx(const char* url, bool ssl_verify): abrt_xmlrpc_conn(url, ssl_verify)
                { xmlrpc_env_init(&env); }
    ~ctx() { xmlrpc_env_clean(&env); }

    void login(const char* login, const char* passwd);
    void logout();

    const char* get_bug_status(xmlrpc_value* result_xml);
    const char* get_bug_resolution(xmlrpc_value* result_xml);
    const char* get_bug_reporter(xmlrpc_value* result_xml);
    const char* get_bug_product(xmlrpc_value* relult_xml);

    xmlrpc_value* call_quicksearch_duphash(const char* component, const char* release, const char* duphash);
    xmlrpc_value* get_cc_member(xmlrpc_value* result_xml);
    xmlrpc_value* get_member(const char* member, xmlrpc_value* result_xml);

    int          get_array_size(xmlrpc_value* result_xml);
    xmlrpc_int32 get_bug_id(xmlrpc_value* result_xml);
    xmlrpc_int32 get_bug_dup_id(xmlrpc_value* result_xml);
    void         get_bug_cc(xmlrpc_value* result_xml, struct bug_info* bz);
    int          add_plus_one_cc(xmlrpc_int32 bug_id, const char* login);
    xmlrpc_int32 new_bug(const map_crash_data_t& pCrashData, int depend_on_bugno);
    int          add_attachments(const char* bug_id_str, const map_crash_data_t& pCrashData);
    int          get_bug_info(struct bug_info* bz, xmlrpc_int32 bug_id);
    int          add_comment(xmlrpc_int32 bug_id, const char* comment, bool is_private);

    xmlrpc_value* call(const char* method, const char* format, ...);
};

xmlrpc_value* ctx::call(const char* method, const char* format, ...)
{
    va_list args;
    xmlrpc_value* param = NULL;
    xmlrpc_value* result = NULL;
    const char* suffix;

    va_start(args, format);
    xmlrpc_build_value_va(&env, format, args, &param, &suffix);
    va_end(args);

    if (!env.fault_occurred)
    {
        if (*suffix != '\0')
        {
            xmlrpc_env_set_fault_formatted(
                &env, XMLRPC_INTERNAL_ERROR, "Junk after the argument "
                "specifier: '%s'.  There must be exactly one arument.",
                suffix);

            xmlrpc_DECREF(param);
            return NULL;
        }

        xmlrpc_client_call2(&env, m_pClient, m_pServer_info, method, param, &result);
        xmlrpc_DECREF(param);
        if (env.fault_occurred)
            return NULL;
    }


    return result;
}

xmlrpc_value* ctx::get_member(const char* member, xmlrpc_value* result_xml)
{
    xmlrpc_value* cc_member = NULL;
    xmlrpc_struct_find_value(&env, result_xml, member, &cc_member);
    if (env.fault_occurred)
        return NULL;

    return cc_member;
}

int ctx::get_array_size(xmlrpc_value* result_xml)
{
    int size = xmlrpc_array_size(&env, result_xml);
    if (env.fault_occurred)
        return -1;

    return size;
}

xmlrpc_int32 ctx::get_bug_dup_id(xmlrpc_value* result_xml)
{
    xmlrpc_value* dup_id = get_member("dup_id", result_xml);
    if (!dup_id)
        return -1;

    xmlrpc_int32 dup_id_int = -1;
    xmlrpc_read_int(&env, dup_id, &dup_id_int);
    xmlrpc_DECREF(dup_id);
    if (env.fault_occurred)
        return -1;

    VERB3 log("got dup_id: %i", dup_id_int);
    return dup_id_int;
}

const char* ctx::get_bug_product(xmlrpc_value* result_xml)
{
    xmlrpc_value* product_member = get_member("product", result_xml);
    if (!product_member) //should never happend. Each bug has to set up product
        return NULL;

    const char* product = NULL;
    xmlrpc_read_string(&env, product_member, &product);
    xmlrpc_DECREF(product_member);
    if (env.fault_occurred)
        return NULL;

    if (*product != '\0')
    {
        VERB3 log("got bug product: %s", product);
        return product;
    }

    free((void*)product);
    return NULL;
}

const char* ctx::get_bug_reporter(xmlrpc_value* result_xml)
{
    xmlrpc_value* reporter_member = get_member("reporter", result_xml);
    if (!reporter_member)
        return NULL;

    const char* reporter = NULL;
    xmlrpc_read_string(&env, reporter_member, &reporter);
    xmlrpc_DECREF(reporter_member);
    if (env.fault_occurred)
        return NULL;

    if (*reporter != '\0')
    {
        VERB3 log("got bug reporter: %s", reporter);
        return reporter;
    }
    free((void*)reporter);
    return NULL;
}

const char* ctx::get_bug_resolution(xmlrpc_value* result_xml)
{
    xmlrpc_value* bug_resolution = get_member("resolution", result_xml);
    if (!bug_resolution)
        return NULL;

    const char* resolution_str = NULL;
    xmlrpc_read_string(&env, bug_resolution, &resolution_str);
    xmlrpc_DECREF(bug_resolution);
    if (env.fault_occurred)
        return NULL;

    if (*resolution_str != '\0')
    {
        VERB3 log("got resolution: %s", resolution_str);
        return resolution_str;
    }
    free((void*)resolution_str);
    return NULL;
}

const char* ctx::get_bug_status(xmlrpc_value* result_xml)
{
    xmlrpc_value* bug_status = get_member("bug_status", result_xml);
    if (!bug_status)
        return NULL;

    const char* status_str = NULL;
    xmlrpc_read_string(&env, bug_status, &status_str);
    xmlrpc_DECREF(bug_status);
    if (env.fault_occurred)
        return NULL;

    if (*status_str != '\0')
    {
        VERB3 log("got bug_status: %s", status_str);
        return status_str;
    }
    free((void*)status_str);
    return NULL;
}

void ctx::get_bug_cc(xmlrpc_value* result_xml, struct bug_info* bz)
{
    xmlrpc_value* cc_member = get_member("cc", result_xml);
    if (!cc_member)
        return;

    int array_size = xmlrpc_array_size(&env, cc_member);
    if (array_size == -1)
        return;

    VERB3 log("count members on cc %i", array_size);

    for (int i = 0; i < array_size; i++)
    {
        xmlrpc_value* item = NULL;
        xmlrpc_array_read_item(&env, cc_member, i, &item);
        if (env.fault_occurred)
            return;

        if (item)
        {
            const char* cc = NULL;
            xmlrpc_read_string(&env, item, &cc);
            xmlrpc_DECREF(item);
            if (env.fault_occurred)
            {
                xmlrpc_DECREF(cc_member);
                return;
            }

            if (*cc != '\0')
            {
                bz->bug_cc.push_back(cc);
                VERB3 log("member on cc is %s", cc);
                continue;
            }
            free((void*)cc);
        }
    }
    xmlrpc_DECREF(cc_member);
    return;
}

xmlrpc_value* ctx::call_quicksearch_duphash(const char* component, const char* release, const char* duphash)
{
    char *query = NULL;
    if (!release)
        query = xasprintf("ALL component:\"%s\" whiteboard:\"%s\"", component, duphash);
    else
    {
        char *product = NULL;
        char *version = NULL;
        parse_release(release, &product, &version);
        query = xasprintf("ALL component:\"%s\" whiteboard:\"%s\" product:\"%s\"",
                                                            component, duphash, product
        );
        free(product);
        free(version);
    }

    VERB3 log("quicksearch for `%s'", query);
    xmlrpc_value *ret = call("Bug.search", "({s:s})", "quicksearch", query);
    free(query);
    return ret;
}

xmlrpc_int32 ctx::get_bug_id(xmlrpc_value* result_xml)
{
    xmlrpc_value* item = NULL;
    xmlrpc_array_read_item(&env, result_xml, 0, &item);
    if (env.fault_occurred)
        return -1;

    xmlrpc_value* bug = get_member("bug_id", item);
    xmlrpc_DECREF(item);
    if (!bug)
        return -1;

    xmlrpc_int32 bug_id = -1;
    xmlrpc_read_int(&env, bug, &bug_id);
    xmlrpc_DECREF(bug);
    if (env.fault_occurred)
        return -1;

    VERB3 log("got bug_id %d", (int)bug_id);
    return bug_id;
}

int ctx::add_plus_one_cc(xmlrpc_int32 bug_id, const char* login)
{
    xmlrpc_value* result = call("Bug.update", "({s:i,s:{s:(s)}})", "ids", (int)bug_id, "updates", "add_cc", login);
    if (result)
        xmlrpc_DECREF(result);
    return result ? 0 : -1;
}

int ctx::add_comment(xmlrpc_int32 bug_id, const char* comment, bool is_private)
{
    xmlrpc_value* result = call("Bug.add_comment", "({s:i,s:s,s:b})", "id", (int)bug_id,
                                                                      "comment", comment,
                                                                      "private", is_private);
    if (result)
        xmlrpc_DECREF(result);
    return result ? 0 : -1;
}

xmlrpc_int32 ctx::new_bug(const map_crash_data_t& pCrashData, int depend_on_bugno)
{
    const char *package         = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_PACKAGE);
    const char *component       = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_COMPONENT);
    const char *release         = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_RELEASE);
    const char *arch            = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_ARCHITECTURE);
    const char *duphash         = get_crash_data_item_content_or_NULL(pCrashData, CD_DUPHASH);
    const char *reason          = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_REASON);
    const char *function        = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_CRASH_FUNCTION);

    struct strbuf *buf_summary = strbuf_new();
    strbuf_append_strf(buf_summary, "[abrt] %s", package);

    if (function != NULL && strlen(function) < 30)
        strbuf_append_strf(buf_summary, ": %s", function);

    if (reason != NULL)
        strbuf_append_strf(buf_summary, ": %s", reason);

    char *status_whiteboard = xasprintf("abrt_hash:%s", duphash);

    char *bz_dsc = make_description_bz(pCrashData);
    char *full_dsc = xasprintf("abrt version: "VERSION"\n%s", bz_dsc);
    free(bz_dsc);

    char *product = NULL;
    char *version = NULL;
    parse_release(release, &product, &version);

    xmlrpc_value* result = NULL;
    char *summary = strbuf_free_nobuf(buf_summary);
    if (depend_on_bugno > -1)
    {
        result = call("Bug.create", "({s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:i})",
                                "product", product,
                                "component", component,
                                "version", version,
                                "summary", summary,
                                "description", full_dsc,
                                "status_whiteboard", status_whiteboard,
                                "platform", arch,
                                "dependson", depend_on_bugno
                              );
    }
    else
    {
        result = call("Bug.create", "({s:s,s:s,s:s,s:s,s:s,s:s,s:s})",
                                "product", product,
                                "component", component,
                                "version", version,
                                "summary", summary,
                                "description", full_dsc,
                                "status_whiteboard", status_whiteboard,
                                "platform", arch
                              );
    }
    free(status_whiteboard);
    free(product);
    free(version);
    free(summary);
    free(full_dsc);

    if (!result)
        return -1;

    xmlrpc_value* id = get_member("id", result);
    xmlrpc_DECREF(result);
    if (!id)
        return -1;

    xmlrpc_int32 bug_id = -1;
    xmlrpc_read_int(&env, id, &bug_id);
    xmlrpc_DECREF(id);
    if (env.fault_occurred)
        return -1;

    log("New bug id: %i", (int)bug_id);
    update_client(_("New bug id: %i"), (int)bug_id);

    return bug_id;
}

int ctx::add_attachments(const char* bug_id_str, const map_crash_data_t& pCrashData)
{
    map_crash_data_t::const_iterator it = pCrashData.begin();
    for (; it != pCrashData.end(); it++)
    {
        const char *itemname = it->first.c_str();
        const char *type = it->second[CD_TYPE].c_str();
        const char *content = it->second[CD_CONTENT].c_str();

        if ((strcmp(type, CD_TXT) == 0)
         && (strlen(content) > CD_TEXT_ATT_SIZE || (strcmp(itemname, FILENAME_BACKTRACE) == 0))
        ) {
            char *encoded64 = encode_base64(content, strlen(content));
            char *filename = xasprintf("File: %s", itemname);
            xmlrpc_value* result = call("bugzilla.addAttachment", "(s{s:s,s:s,s:s,s:s})", bug_id_str,
                                        "description", filename,
                                        "filename", itemname,
                                        "contenttype", "text/plain",
                                        "data", encoded64
                                      );
            free(encoded64);
            free(filename);
            if (!result)
                return -1;

            xmlrpc_DECREF(result);
        }
    }
    return 0;
}

int ctx::get_bug_info(struct bug_info* bz, xmlrpc_int32 bug_id)
{
    xmlrpc_value* result = call("bugzilla.getBug", "(s)", to_string(bug_id).c_str());
    if (!result)
        return -1;

    if (result)
    {
        bz->bug_product = get_bug_product(result);
        if (bz->bug_product == NULL)
            return -1;

        bz->bug_status = get_bug_status(result);
        if (bz->bug_status == NULL)
            return -1;

        bz->bug_reporter = get_bug_reporter(result);
        if (bz->bug_reporter == NULL)
            return -1;

        // mandatory when bug status is CLOSED
        if (strcmp(bz->bug_status, "CLOSED") == 0)
        {
            bz->bug_resolution = get_bug_resolution(result);
            if ((env.fault_occurred) && (bz->bug_resolution == NULL))
                return -1;
        }

        // mandatory when bug status is CLOSED and resolution is DUPLICATE
        if ((strcmp(bz->bug_status, "CLOSED") == 0)
         && (strcmp(bz->bug_resolution, "DUPLICATE") == 0)
        ) {
            bz->bug_dup_id = get_bug_dup_id(result);
            if (env.fault_occurred)
                return -1;
        }

        get_bug_cc(result, bz);
        if (env.fault_occurred)
            return -1;

        xmlrpc_DECREF(result);
        return 0;
     }
     return -1;
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
    xmlrpc_value* result = call("User.login", "({s:s,s:s})", "login", login, "password", passwd);

    if (!result)
    {
        char *errmsg = xasprintf("Can't login. Check Edit->Plugins->Bugzilla and /etc/abrt/plugins/Bugzilla.conf. Server said: %s", env.fault_string);
        error_msg("%s", errmsg); // show error in daemon log
        CABRTException e(EXCEP_PLUGIN,  errmsg);
        free(errmsg);
        throw e;
    }
    xmlrpc_DECREF(result);
}

void ctx::logout()
{
    xmlrpc_value* result = call("User.logout", "(s)", "");
    if (result)
        xmlrpc_DECREF(result);

    throw_if_xml_fault_occurred(&env);
}

} /* namespace */


/*
 * CReporterBugzilla
 */

static map_plugin_settings_t parse_settings(const map_plugin_settings_t& pSettings)
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

    it = pSettings.find("SSLVerify");
    if (it == end)
    {
        plugin_settings.clear();
        return plugin_settings;
    }
    plugin_settings["SSLVerify"] = it->second;

    VERB1 log("User settings ok, using them instead of defaults");
    return plugin_settings;
}

CReporterBugzilla::CReporterBugzilla()
{
    m_ssl_verify = true;
    m_rating_required = true;
    m_login = NULL;
    m_password = NULL;
    m_bugzilla_url = xstrdup("https://bugzilla.redhat.com");
    m_bugzilla_xmlrpc = xstrdup("https://bugzilla.redhat.com"XML_RPC_SUFFIX);
}

CReporterBugzilla::~CReporterBugzilla()
{
    free(m_login);
    free(m_password);
    free(m_bugzilla_url);
    free(m_bugzilla_xmlrpc);
}

std::string CReporterBugzilla::Report(const map_crash_data_t& pCrashData,
                                      const map_plugin_settings_t& pSettings,
                                      const char *pArgs)
{
    xmlrpc_int32 bug_id = -1;
    const char *login = NULL;
    const char *password = NULL;
    const char *bugzilla_xmlrpc = NULL;
    const char *bugzilla_url = NULL;
    bool ssl_verify;

    map_plugin_settings_t settings = parse_settings(pSettings);
    /* if parse_settings fails it returns an empty map so we need to use defaults */
    if (!settings.empty())
    {
        login = settings["Login"].c_str();
        password = settings["Password"].c_str();
        bugzilla_xmlrpc = settings["BugzillaXMLRPC"].c_str();
        bugzilla_url = settings["BugzillaURL"].c_str();
        ssl_verify = string_to_bool(settings["NoSSLVerify"].c_str());
    }
    else
    {
        login = m_login;
        password = m_password;
        bugzilla_xmlrpc = m_bugzilla_xmlrpc;
        bugzilla_url = m_bugzilla_url;
        ssl_verify = m_ssl_verify;
    }

    if (!login[0] || !password[0])
    {
        VERB3 log("Empty login and password");
        throw CABRTException(EXCEP_PLUGIN, _("Empty login or password.\nPlease check "PLUGINS_CONF_DIR"/Bugzilla.conf."));
    }

    const char *component = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_COMPONENT);
    const char *duphash   = get_crash_data_item_content_or_NULL(pCrashData, CD_DUPHASH);
    const char *release   = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_RELEASE);

    ctx bz_server(bugzilla_xmlrpc, ssl_verify);

    update_client(_("Logging into bugzilla..."));
    bz_server.login(login, password);

    update_client(_("Checking for duplicates..."));

    char *product = NULL;
    char *version = NULL;
    parse_release(release, &product, &version);

    xmlrpc_value *result;
    if (strcmp(product, "Fedora") == 0)
        result  = bz_server.call_quicksearch_duphash(component, product, duphash);
    else
        result  = bz_server.call_quicksearch_duphash(component, NULL, duphash);

    if (!result)
        throw_if_xml_fault_occurred(&bz_server.env);

    xmlrpc_value *all_bugs = bz_server.get_member("bugs", result);
    xmlrpc_DECREF(result);

    if (!all_bugs)
    {
        throw_if_xml_fault_occurred(&bz_server.env);
        throw CABRTException(EXCEP_PLUGIN, _("Missing mandatory member 'bugs'"));
    }

    int all_bugs_size = bz_server.get_array_size(all_bugs);
    struct bug_info bz;
    int depend_on_bugno = -1;
    if (all_bugs_size > 0)
    {
        bug_id = bz_server.get_bug_id(all_bugs);
        xmlrpc_DECREF(all_bugs);
        if (bug_id == -1)
            throw_if_xml_fault_occurred(&bz_server.env);

        bug_info_init(&bz);
        if (bz_server.get_bug_info(&bz, bug_id) == -1)
        {
            bug_info_destroy(&bz);
            throw_if_xml_fault_occurred(&bz_server.env);
            throw CABRTException(EXCEP_PLUGIN, _("get_bug_info() failed. Could not collect all mandatory information"));
        }

        if (strcmp(bz.bug_product, product) != 0)
        {
            depend_on_bugno = bug_id;
            bug_info_destroy(&bz);
            result = bz_server.call_quicksearch_duphash(component, release, duphash);
            if (!result)
                throw_if_xml_fault_occurred(&bz_server.env);

            all_bugs = bz_server.get_member("bugs", result);
            xmlrpc_DECREF(result);

            if (!all_bugs)
            {
                throw_if_xml_fault_occurred(&bz_server.env);
                throw CABRTException(EXCEP_PLUGIN, _("Missing mandatory member 'bugs'"));
            }

            all_bugs_size = bz_server.get_array_size(all_bugs);
            if (all_bugs_size > 0)
            {
                bug_id = bz_server.get_bug_id(all_bugs);
                xmlrpc_DECREF(all_bugs);
                if (bug_id == -1)
                    throw_if_xml_fault_occurred(&bz_server.env);

                bug_info_init(&bz);
                if (bz_server.get_bug_info(&bz, bug_id) == -1)
                {
                    bug_info_destroy(&bz);
                    throw_if_xml_fault_occurred(&bz_server.env);
                    throw CABRTException(EXCEP_PLUGIN, _("get_bug_info() failed. Could not collect all mandatory information"));
                }
            }
            else
                xmlrpc_DECREF(all_bugs);
        }
    }
    free(product);
    free(version);

    if (all_bugs_size < 0)
    {
        throw_if_xml_fault_occurred(&bz_server.env);
    }
    else if (all_bugs_size == 0) // Create new bug
    {
        update_client(_("Creating a new bug..."));
        bug_id = bz_server.new_bug(pCrashData, depend_on_bugno);
        if (bug_id < 0)
        {
            throw_if_xml_fault_occurred(&bz_server.env);
            throw CABRTException(EXCEP_PLUGIN, _("Bugzilla entry creation failed"));
        }

        log("Adding attachments to bug %d...", bug_id);
        int ret = bz_server.add_attachments(to_string(bug_id).c_str(), pCrashData);
        if (ret == -1)
        {
            throw_if_xml_fault_occurred(&bz_server.env);
        }

        update_client(_("Logging out..."));
        bz_server.logout();

        std::string bug_status = ssprintf(
                    "Status: NEW\n"
                    "%s/show_bug.cgi?id=%u",
                    bugzilla_url,
                    (int)bug_id
        );
        return bug_status;
    }
    else if (all_bugs_size > 1)
    {
        // When someone clones bug it has same duphash, so we can find more than 1.
        // Need to be checked if component is same.
        VERB3 log("Bugzilla has %u reports with same duphash '%s'", all_bugs_size, duphash);
    }

    // decision based on state
    update_client(_("Bug is already reported: %i"), bug_id);

    xmlrpc_int32 original_bug_id = bug_id;
    if ((strcmp(bz.bug_status, "CLOSED") == 0) && (strcmp(bz.bug_resolution, "DUPLICATE") == 0))
    {
        for (int ii = 0; ii <= MAX_HOPS; ii++)
        {
            if (ii == MAX_HOPS)
            {
                VERB3 log("Bugzilla could not find a parent of bug %d", (int)original_bug_id);
                bug_info_destroy(&bz);
                throw CABRTException(EXCEP_PLUGIN, _("Bugzilla couldn't find parent of bug %d"), (int)original_bug_id);
            }

            log("Bug %d is a duplicate, using parent bug %d", bug_id, (int)bz.bug_dup_id);
            bug_id = bz.bug_dup_id;
            bug_info_destroy(&bz);
            bug_info_init(&bz);

            if (bz_server.get_bug_info(&bz, bug_id) == -1)
            {
                bug_info_destroy(&bz);
                if (bz_server.env.fault_occurred)
                {
                    throw_if_xml_fault_occurred(&bz_server.env);
                }
                throw CABRTException(EXCEP_PLUGIN, _("get_bug_info() failed. Could not collect all mandatory information"));
            }

            // found a bug which is not CLOSED as DUPLICATE
            if (bz.bug_dup_id == -1)
                break;
        }
    }

    if (strcmp(bz.bug_status, "CLOSED") != 0)
    {
        int status = 0;
        if ((strcmp(bz.bug_reporter, login) != 0) && (am_i_in_cc(&bz, login)))
        {
            VERB2 log(_("Add %s to CC list"), login);
            update_client(_("Add %s to CC list"), login);
            status = bz_server.add_plus_one_cc(bug_id, login);
        }

        if (status == -1)
        {
            bug_info_destroy(&bz);
            throw_if_xml_fault_occurred(&bz_server.env);
        }

        char *dsc = make_description_reproduce_comment(pCrashData);
        if (dsc)
        {
            const char* package    = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_PACKAGE);
            const char* release    = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_RELEASE);
            const char* arch       = get_crash_data_item_content_or_NULL(pCrashData, FILENAME_ARCHITECTURE);
            const char* is_private = get_crash_data_item_content_or_NULL(pCrashData, "is_private");

            char *full_dsc = xasprintf("Package: %s\n"
                                "Architecture: %s\n"
                                "OS Release: %s\n"
                                "%s", package, arch, release, dsc
            );

            update_client(_("Adding new comment to bug %d"), (int)bug_id);

            free(dsc);

            bool is_priv = is_private && (is_private[0] == '1');
            if (bz_server.add_comment(bug_id, full_dsc, is_priv) == -1)
            {
                free(full_dsc);
                bug_info_destroy(&bz);
                throw_xml_fault(&bz_server.env);
            }
            free(full_dsc);
        }
    }

    update_client(_("Logging out..."));
    bz_server.logout();

    std::string bug_status = ssprintf(
                "Status: %s%s%s\n"
                "%s/show_bug.cgi?id=%u",
                bz.bug_status,
                bz.bug_resolution ? " " : "",
                bz.bug_resolution ? bz.bug_resolution : "",
                bugzilla_url,
                (int)bug_id
    );

    bug_info_destroy(&bz);

    return bug_status;
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
        free(m_bugzilla_url);
        free(m_bugzilla_xmlrpc);

        m_bugzilla_url = xstrdup(it->second.c_str());

        int cnt = strlen(m_bugzilla_url);
        while (m_bugzilla_url[cnt--] == '/')
            m_bugzilla_url[cnt] = '\0';

        int ret = suffixcmp(m_bugzilla_url, XML_RPC_SUFFIX);
        if (ret != 0)
            m_bugzilla_xmlrpc = xasprintf("%s%s", m_bugzilla_url, XML_RPC_SUFFIX);
        else
            m_bugzilla_xmlrpc = xstrdup(m_bugzilla_url);
    }
    it = pSettings.find("Login");
    if (it != end)
    {
        free(m_login);
        m_login = xstrdup(it->second.c_str());
    }
    it = pSettings.find("Password");
    if (it != end)
    {
        free(m_password);
        m_password = xstrdup(it->second.c_str());
    }
    it = pSettings.find("SSLVerify");
    if (it != end)
    {
        m_ssl_verify = string_to_bool(it->second.c_str());
    }
}

/* Should not be deleted (why?) */
const map_plugin_settings_t& CReporterBugzilla::GetSettings()
{
    m_pSettings["BugzillaURL"] = (m_bugzilla_url)? m_bugzilla_url: "";
    m_pSettings["Login"] = (m_login)? m_login: "";
    m_pSettings["Password"] = (m_password)? m_password: "";
    m_pSettings["SSLVerify"] = m_ssl_verify ? "yes" : "no";
    m_pSettings["RatingRequired"] = m_rating_required ? "yes" : "no";

    return m_pSettings;
}

PLUGIN_INFO(REPORTER,
            CReporterBugzilla,
            "Bugzilla",
            "0.0.4",
            _("Reports bugs to bugzilla"),
            "npajkovs@redhat.com",
            "https://fedorahosted.org/abrt/wiki",
            PLUGINS_LIB_DIR"/Bugzilla.glade");
