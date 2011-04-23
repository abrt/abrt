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
#include "abrt_problem_data.h"
#include "parse_options.h"

#define PROGNAME "abrt-action-bugzilla"

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
    GList* bug_cc;
};

/* xzalloc */
static void bug_info_init(struct bug_info* bz)
{
    bz->bug_status = NULL;
    bz->bug_resolution = NULL;
    bz->bug_reporter = NULL;
    bz->bug_product = NULL;
    bz->bug_dup_id = -1;
    bz->bug_cc = NULL;
}

static void bug_info_destroy(struct bug_info* bz)
{
    free((void*)bz->bug_status);
    free((void*)bz->bug_resolution);
    free((void*)bz->bug_reporter);
    free((void*)bz->bug_product);

    list_free_with_free(bz->bug_cc);
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
    xmlrpc_int32 new_bug(problem_data_t *problem_data, int depend_on_bugno);
    int          add_attachments(const char* bug_id_str, problem_data_t *problem_data);
    int          get_bug_info(struct bug_info* bz, xmlrpc_int32 bug_id);
    int          add_comment(xmlrpc_int32 bug_id, const char* comment, bool is_private);

    xmlrpc_value* call(const char* method, const char* format, ...);
};

xmlrpc_value* ctx::call(const char* method, const char* format, ...)
{
    xmlrpc_value* result = NULL;

    if (!env.fault_occurred)
    {
        xmlrpc_value* param = NULL;
        va_list args;
        const char* suffix;

        va_start(args, format);
        xmlrpc_build_value_va(&env, format, args, &param, &suffix);
        va_end(args);

        if (*suffix != '\0')
        {
            xmlrpc_env_set_fault_formatted(
                &env, XMLRPC_INTERNAL_ERROR, "Junk after the argument "
                "specifier: '%s'.  There must be exactly one arument.",
                suffix);
        }
        else
        {
            xmlrpc_client_call2(&env, m_pClient, m_pServer_info, method, param, &result);
        }
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
                bz->bug_cc = g_list_append(bz->bug_cc, (char*)cc);
                VERB3 log("member on cc is %s", cc);
                continue;
            }
            free((char*)cc);
        }
    }
    xmlrpc_DECREF(cc_member);
    return;
}

xmlrpc_value* ctx::call_quicksearch_duphash(const char* component,
                                            const char* release, const char* duphash)
{
    char *query = NULL;
    if (!release)
        query = xasprintf("ALL component:\"%s\" whiteboard:\"%s\"", component, duphash);
    else
    {
        char *product = NULL;
        char *version = NULL;
        parse_release_for_bz(release, &product, &version);
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

/* From RHEL6 kernel/panic.c:
 * { TAINT_PROPRIETARY_MODULE,     'P', 'G' },
 * { TAINT_FORCED_MODULE,          'F', ' ' },
 * { TAINT_UNSAFE_SMP,             'S', ' ' },
 * { TAINT_FORCED_RMMOD,           'R', ' ' },
 * { TAINT_MACHINE_CHECK,          'M', ' ' },
 * { TAINT_BAD_PAGE,               'B', ' ' },
 * { TAINT_USER,                   'U', ' ' },
 * { TAINT_DIE,                    'D', ' ' },
 * { TAINT_OVERRIDDEN_ACPI_TABLE,  'A', ' ' },
 * { TAINT_WARN,                   'W', ' ' },
 * { TAINT_CRAP,                   'C', ' ' },
 * { TAINT_FIRMWARE_WORKAROUND,    'I', ' ' },
 * entries 12 - 27 are unused
 * { TAINT_HARDWARE_UNSUPPORTED,   'H', ' ' },
 * entries 29 - 31 are unused
 */

static const char * const taint_warnings[] = {
    "Proprietary Module",
    "Forced Module",
    "Unsafe SMP",
    "Forced rmmod",
    "Machine Check",
    "Bad Page",
    "User",
    "Die",
    "Overriden ACPI Table",
    "Warning Issued",
    "Experimental Module Loaded",
    "Firmware Workaround",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "Hardware Unsupported",
    NULL,
    NULL,
};

static const char *tainted_string(unsigned tainted)
{
    unsigned idx = 0;
    while ((tainted >>= 1) != 0)
        idx++;

    return taint_warnings[idx];
}

xmlrpc_int32 ctx::new_bug(problem_data_t *problem_data, int depend_on_bugno)
{
    const char *package         = get_problem_item_content_or_NULL(problem_data, FILENAME_PACKAGE);
    const char *component       = get_problem_item_content_or_NULL(problem_data, FILENAME_COMPONENT);
    const char *release         = get_problem_item_content_or_NULL(problem_data, FILENAME_OS_RELEASE);
    if (!release) /* Old dump dir format compat. Remove in abrt-2.1 */
        release = get_problem_item_content_or_NULL(problem_data, "release");
    const char *arch            = get_problem_item_content_or_NULL(problem_data, FILENAME_ARCHITECTURE);
    const char *duphash         = get_problem_item_content_or_NULL(problem_data, FILENAME_DUPHASH);
    const char *reason          = get_problem_item_content_or_NULL(problem_data, FILENAME_REASON);
    const char *function        = get_problem_item_content_or_NULL(problem_data, FILENAME_CRASH_FUNCTION);
    const char *analyzer        = get_problem_item_content_or_NULL(problem_data, FILENAME_ANALYZER);
    const char *tainted_str     = get_problem_item_content_or_NULL(problem_data, FILENAME_TAINTED);

    struct strbuf *buf_summary = strbuf_new();
    strbuf_append_strf(buf_summary, "[abrt] %s", package);

    if (function != NULL && strlen(function) < 30)
        strbuf_append_strf(buf_summary, ": %s", function);

    if (reason != NULL)
        strbuf_append_strf(buf_summary, ": %s", reason);

    if (tainted_str && analyzer
        && (strcmp(analyzer, "Kerneloops") == 0)
    ) {
        unsigned long tainted = xatoi_positive(tainted_str);
        const char *tainted_warning = tainted_string(tainted);
        if (tainted_warning)
            strbuf_append_strf(buf_summary, ": TAINTED %s", tainted_warning);
    }

    char *status_whiteboard = xasprintf("abrt_hash:%s", duphash);

    char *bz_dsc = make_description_bz(problem_data);
    char *full_dsc = xasprintf("abrt version: "VERSION"\n%s", bz_dsc);
    free(bz_dsc);

    char *product = NULL;
    char *version = NULL;
    parse_release_for_bz(release, &product, &version);

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

    log(_("New bug id: %i"), (int)bug_id);

    return bug_id;
}

int ctx::add_attachments(const char* bug_id_str, problem_data_t *problem_data)
{
    GHashTableIter iter;
    char *name;
    struct problem_item *value;
    g_hash_table_iter_init(&iter, problem_data);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        const char *content = value->content;

        if ((value->flags & CD_FLAG_TXT)
         && (strlen(content) > CD_TEXT_ATT_SIZE || (strcmp(name, FILENAME_BACKTRACE) == 0))
        ) {
            char *encoded64 = encode_base64(content, strlen(content));
            char *filename = xasprintf("File: %s", name);
            xmlrpc_value* result = call("bugzilla.addAttachment", "(s{s:s,s:s,s:s,s:s})", bug_id_str,
                                        "description", filename,
                                        "filename", name,
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
    char bug_id_str[sizeof(long)*3 + 2];
    sprintf(bug_id_str, "%lu", (long)bug_id);
    xmlrpc_value* result = call("bugzilla.getBug", "(s)", bug_id_str);
    if (!result)
        return -1;

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

void ctx::login(const char* login, const char* passwd)
{
    xmlrpc_value* result = call("User.login", "({s:s,s:s})", "login", login, "password", passwd);
//TODO: with URL like http://bugzilla.redhat.com (that is, with http: instead of https:)
//we are getting this error:
//Logging into Bugzilla at http://bugzilla.redhat.com
//Can't login. Server said: HTTP response code is 301, not 200
//But this is a 301 redirect! We _can_ follow it if we configure curl to understand that!
    if (!result)
        error_msg_and_die("Can't login. Server said: %s", env.fault_string);
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


static void report_to_bugzilla(
                const char *dump_dir_name,
                map_string_h *settings)
{
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (!dd)
        xfunc_die(); /* dd_opendir already emitted error msg */
    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
    dd_close(dd);

    const char *env;
    const char *login;
    const char *password;
    const char *bugzilla_xmlrpc;
    const char *bugzilla_url;
    bool ssl_verify;

    env = getenv("Bugzilla_Login");
    login = env ? env : get_map_string_item_or_empty(settings, "Login");
    env = getenv("Bugzilla_Password");
    password = env ? env : get_map_string_item_or_empty(settings, "Password");
    if (!login[0] || !password[0])
        error_msg_and_die(_("Empty login or password, please check your configuration"));

    env = getenv("Bugzilla_BugzillaURL");
    bugzilla_url = env ? env : get_map_string_item_or_empty(settings, "BugzillaURL");
    if (!bugzilla_url[0])
        bugzilla_url = "https://bugzilla.redhat.com";
    bugzilla_xmlrpc = xasprintf("%s"XML_RPC_SUFFIX, bugzilla_url);

    env = getenv("Bugzilla_SSLVerify");
    ssl_verify = string_to_bool(env ? env : get_map_string_item_or_empty(settings, "SSLVerify"));

    const char *component = get_problem_item_content_or_NULL(problem_data, FILENAME_COMPONENT);
    const char *duphash   = get_problem_item_content_or_NULL(problem_data, FILENAME_DUPHASH);
    if (!duphash)
        error_msg_and_die(_("Essential file '%s' is missing, can't continue.."),
                          FILENAME_DUPHASH);

    if (!*duphash)
        error_msg_and_die(_("Essential file '%s' is empty, can't continue.."),
                          FILENAME_DUPHASH);

    const char *release   = get_problem_item_content_or_NULL(problem_data, FILENAME_OS_RELEASE);
    if (!release) /* Old dump dir format compat. Remove in abrt-2.1 */
        release = get_problem_item_content_or_NULL(problem_data, "release");

    ctx bz_server(bugzilla_xmlrpc, ssl_verify);

    log(_("Logging into Bugzilla at %s"), bugzilla_url);
    bz_server.login(login, password);

    log(_("Checking for duplicates"));

    char *product = NULL;
    char *version = NULL;
    parse_release_for_bz(release, &product, &version);
    free(version);

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
        error_msg_and_die(_("Missing mandatory member 'bugs'"));
    }

    xmlrpc_int32 bug_id = -1;
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
            error_msg_and_die(_("get_bug_info() failed. Could not collect all mandatory information"));
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
                error_msg_and_die(_("Missing mandatory member 'bugs'"));
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
                    error_msg_and_die(_("get_bug_info() failed. Could not collect all mandatory information"));
                }
            }
            else
                xmlrpc_DECREF(all_bugs);
        }
    }
    free(product);

    if (all_bugs_size < 0)
    {
        throw_if_xml_fault_occurred(&bz_server.env);
    }
    else if (all_bugs_size == 0) // Create new bug
    {
        log(_("Creating a new bug"));
        bug_id = bz_server.new_bug(problem_data, depend_on_bugno);
        if (bug_id < 0)
        {
            throw_if_xml_fault_occurred(&bz_server.env);
            error_msg_and_die(_("Bugzilla entry creation failed"));
        }

        log("Adding attachments to bug %ld", (long)bug_id);
        char bug_id_str[sizeof(long)*3 + 2];
        sprintf(bug_id_str, "%ld", (long) bug_id);
        int ret = bz_server.add_attachments(bug_id_str, problem_data);
        if (ret == -1)
        {
            throw_if_xml_fault_occurred(&bz_server.env);
        }

        log(_("Logging out"));
        bz_server.logout();

        log("Status: NEW %s/show_bug.cgi?id=%u",
                    bugzilla_url,
                    (int)bug_id
        );
        return;
    }

    if (all_bugs_size > 1)
    {
        // When someone clones bug it has same duphash, so we can find more than 1.
        // Need to be checked if component is same.
        VERB3 log("Bugzilla has %u reports with same duphash '%s'", all_bugs_size, duphash);
    }

    // decision based on state
    log(_("Bug is already reported: %i"), bug_id);

    xmlrpc_int32 original_bug_id = bug_id;
    if ((strcmp(bz.bug_status, "CLOSED") == 0) && (strcmp(bz.bug_resolution, "DUPLICATE") == 0))
    {
        for (int ii = 0; ii <= MAX_HOPS; ii++)
        {
            if (ii == MAX_HOPS)
            {
                VERB3 log("Bugzilla could not find a parent of bug %d", (int)original_bug_id);
                bug_info_destroy(&bz);
                error_msg_and_die(_("Bugzilla couldn't find parent of bug %d"), (int)original_bug_id);
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
                error_msg_and_die(_("get_bug_info() failed. Could not collect all mandatory information"));
            }

            // found a bug which is not CLOSED as DUPLICATE
            if (bz.bug_dup_id == -1)
                break;
        }
    }

    if (strcmp(bz.bug_status, "CLOSED") != 0)
    {
        int status = 0;
        if ((strcmp(bz.bug_reporter, login) != 0)
            && (g_list_find(bz.bug_cc, login)))
        {
            log(_("Add %s to CC list"), login);
            status = bz_server.add_plus_one_cc(bug_id, login);
        }

        if (status == -1)
        {
            bug_info_destroy(&bz);
            throw_if_xml_fault_occurred(&bz_server.env);
        }

        char *dsc = make_description_comment(problem_data);
        if (dsc)
        {
            const char* package    = get_problem_item_content_or_NULL(problem_data, FILENAME_PACKAGE);
            const char* release    = get_problem_item_content_or_NULL(problem_data, FILENAME_OS_RELEASE);
            if (!release) /* Old dump dir format compat. Remove in abrt-2.1 */
                release = get_problem_item_content_or_NULL(problem_data, "release");
            const char* arch       = get_problem_item_content_or_NULL(problem_data, FILENAME_ARCHITECTURE);
            const char* is_private = get_problem_item_content_or_NULL(problem_data, "is_private");

            char *full_dsc = xasprintf("Package: %s\n"
                                "Architecture: %s\n"
                                "OS Release: %s\n"
                                "%s", package, arch, release, dsc
            );

            log(_("Adding new comment to bug %d"), (int)bug_id);

            free(dsc);

            bool is_priv = is_private && string_to_bool(is_private);
            if (bz_server.add_comment(bug_id, full_dsc, is_priv) == -1)
            {
                free(full_dsc);
                bug_info_destroy(&bz);
                throw_xml_fault(&bz_server.env);
            }
            free(full_dsc);
        }
    }

    log(_("Logging out"));
    bz_server.logout();

    log("Status: %s%s%s %s/show_bug.cgi?id=%u",
                bz.bug_status,
                bz.bug_resolution ? " " : "",
                bz.bug_resolution ? bz.bug_resolution : "",
                bugzilla_url,
                (int)bug_id
    );

    dd = dd_opendir(dump_dir_name, /*flags:*/ 0);
    if (dd)
    {
        char *msg = xasprintf("Bugzilla: URL=%s/show_bug.cgi?id=%u", bugzilla_url, (int)bug_id);
        add_reported_to(dd, msg);
        free(msg);
        dd_close(dd);
    }

    free_problem_data(problem_data);
    bug_info_destroy(&bz);
}

int main(int argc, char **argv)
{
    char *env_verbose = getenv("ABRT_VERBOSE");
    if (env_verbose)
        g_verbose = atoi(env_verbose);

    map_string_h *settings = new_map_string();
    const char *dump_dir_name = ".";
    GList *conf_file = NULL;

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        PROGNAME" [-v] -c CONFFILE -d DIR\n"
        "\n"
        "Reports problem to Bugzilla"
    );
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_c = 1 << 2,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_STRING('d', NULL, &dump_dir_name, "DIR" , _("Dump directory")),
        OPT_LIST(  'c', NULL, &conf_file    , "FILE", _("Configuration file (may be given many times)")),
        OPT_END()
    };
    /*unsigned opts =*/ parse_opts(argc, argv, program_options, program_usage_string);

    putenv(xasprintf("ABRT_VERBOSE=%u", g_verbose));

    char *pfx = getenv("ABRT_PROG_PREFIX");
    if (pfx && string_to_bool(pfx))
        msg_prefix = PROGNAME;

    while (conf_file)
    {
        char *fn = (char *)conf_file->data;
        VERB1 log("Loading settings from '%s'", fn);
        load_conf_file(fn, settings, /*skip key w/o values:*/ true);
        VERB3 log("Loaded '%s'", fn);
        conf_file = g_list_remove(conf_file, fn);
    }

    VERB1 log("Initializing XML-RPC library");
    xmlrpc_env env;
    xmlrpc_env_init(&env);
    xmlrpc_client_setup_global_const(&env);
    if (env.fault_occurred)
        error_msg_and_die("XML-RPC Fault: %s(%d)", env.fault_string, env.fault_code);
    xmlrpc_env_clean(&env);

    report_to_bugzilla(dump_dir_name, settings);

    free_map_string(settings);
    return 0;
}
