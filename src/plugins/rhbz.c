/*
    Copyright (C) 2011  ABRT team
    Copyright (C) 2011  RedHat Inc

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
#include "rhbz.h"

#define MAX_HOPS            5

struct bug_info *new_bug_info()
{
    struct bug_info *bi = xzalloc(sizeof(struct bug_info));
    bi->bi_dup_id = -1;

    return bi;
}

void free_bug_info(struct bug_info *bi)
{
    if (!bi)
        return;

    free((void*)bi->bi_status);
    free((void*)bi->bi_resolution);
    free((void*)bi->bi_reporter);
    free((void*)bi->bi_product);

    list_free_with_free(bi->bi_cc_list);

    bi->bi_status = NULL;
    bi->bi_resolution = NULL;
    bi->bi_reporter = NULL;
    bi->bi_product = NULL;

    bi->bi_cc_list = NULL;

    free(bi);
}

void rhbz_login(struct abrt_xmlrpc *ax, const char* login, const char* passwd)
{
    xmlrpc_value* result = abrt_xmlrpc_call(ax, "User.login", "({s:s,s:s})",
                                            "login", login, "password", passwd);

//TODO: with URL like http://bugzilla.redhat.com (that is, with http: instead of https:)
//we are getting this error:
//Logging into Bugzilla at http://bugzilla.redhat.com
//Can't login. Server said: HTTP response code is 301, not 200
//But this is a 301 redirect! We _can_ follow it if we configure curl to understand that!
    xmlrpc_DECREF(result);
}

xmlrpc_value *rhbz_search_duphash(struct abrt_xmlrpc *ax, const char *component,
                                  const char *product, const char *duphash)
{
    char *query = NULL;
    if (!product)
        query = xasprintf("ALL component:\"%s\" whiteboard:\"%s\"", component, duphash);
    else
        query = xasprintf("ALL component:\"%s\" whiteboard:\"%s\" product:\"%s\"",
                          component, duphash, product);

    VERB3 log("search for '%s'", query);
    xmlrpc_value *ret = abrt_xmlrpc_call(ax, "Bug.search", "({s:s})",
                                         "quicksearch", query);
    free(query);
    return ret;
}

xmlrpc_value *rhbz_get_member(const char *member, xmlrpc_value *xml)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value *value = NULL;
    /* The xmlrpc_struct_find_value functions consider "not found" to be
     * a normal result. If a member of the structure with the specified key
     * exists, it returns it as a handle to an xmlrpc_value. If not, it returns
     * NULL in place of that handle.
     */
    xmlrpc_struct_find_value(&env, xml, member, &value);
    if (env.fault_occurred)
        abrt_xmlrpc_error(&env);

    return value;
}

/* The only way this can fail is if arrayP is not actually an array XML-RPC
 * value. So it is usually not worth checking *envP.
 * die or return size of array
 */
int rhbz_array_size(xmlrpc_value *xml)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    int size = xmlrpc_array_size(&env, xml);
    if (env.fault_occurred)
        abrt_xmlrpc_die(&env);

    return size;
}

/* die or return bug id; each bug must have bug id otherwise xml is corrupted */
int rhbz_bug_id(xmlrpc_value* xml)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value *item = NULL;
    xmlrpc_value *bug = NULL;
    int bug_id = -1;;

    xmlrpc_array_read_item(&env, xml, 0, &item);
    if (env.fault_occurred)
        abrt_xmlrpc_die(&env);

    bug = rhbz_get_member("bug_id", item);
    xmlrpc_DECREF(item);
    if (!bug)
        abrt_xmlrpc_die(&env);

    xmlrpc_read_int(&env, bug, &bug_id);
    xmlrpc_DECREF(bug);
    if (env.fault_occurred)
        abrt_xmlrpc_die(&env);

    VERB3 log("found bug_id %i", bug_id);
    return bug_id;
}

/* die when mandatory value is missing (set flag RHBZ_MANDATORY_MEMB)
 * or return appropriate string or NULL when fail;
 */
// TODO: npajkovs: add flag to read xmlrpc_read_array_item first
void *rhbz_bug_read_item(const char *memb, xmlrpc_value *xml, int flags)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value *member = rhbz_get_member(memb, xml);

    const char *string = NULL;

    if (!member)
        goto die;

    if (IS_READ_STR(flags))
    {
        xmlrpc_read_string(&env, member, &string);
        xmlrpc_DECREF(member);
        if (env.fault_occurred)
            abrt_xmlrpc_die(&env);

        if (!*string)
            goto die;

        VERB3 log("found %s: '%s'", memb, string);
        return (void*)string;
    }

    {
        if (IS_READ_INT(flags))
        {
            int *integer = xmalloc(sizeof(int));
            xmlrpc_read_int(&env, member, integer);
            xmlrpc_DECREF(member);
            if (env.fault_occurred)
                abrt_xmlrpc_die(&env);

            VERB3 log("found %s: '%i'", memb, *integer);
            return (void*)integer;
        }
    }
die:
    free((void*)string);
    if (IS_MANDATORY(flags))
        error_msg_and_die(_("Looks like corrupted xml response, because '%s'"
                            " member is missing."), memb);

    return NULL;
}

GList *rhbz_bug_cc(xmlrpc_value* result_xml)
{
    xmlrpc_env env;
    xmlrpc_env_init(&env);

    xmlrpc_value* cc_member = rhbz_get_member("cc", result_xml);
    if (!cc_member)
        return NULL;

    int array_size = rhbz_array_size(cc_member);

    VERB3 log("count members on cc %i", array_size);
    GList *cc_list = NULL;

    for (int i = 0; i < array_size; ++i)
    {
        xmlrpc_value* item = NULL;
        xmlrpc_array_read_item(&env, cc_member, i, &item);
        if (env.fault_occurred)
            abrt_xmlrpc_die(&env);

        if (!item)
            continue;

        const char* cc = NULL;
        xmlrpc_read_string(&env, item, &cc);
        xmlrpc_DECREF(item);
        if (env.fault_occurred)
            abrt_xmlrpc_die(&env);

        if (*cc != '\0')
        {
            cc_list = g_list_append(cc_list, (char*)cc);
            VERB3 log("member on cc is %s", cc);
            continue;
        }
        free((char*)cc);
    }
    xmlrpc_DECREF(cc_member);
    return cc_list;
}

struct bug_info *rhbz_bug_info(struct abrt_xmlrpc *ax, int bug_id)
{
    struct bug_info *bz = new_bug_info();
    xmlrpc_value *xml_bug_response = abrt_xmlrpc_call(ax, "bugzilla.getBug",
                                                      "(i)", bug_id);

    int *ret = (int*)rhbz_bug_read_item("bug_id", xml_bug_response,
                                        RHBZ_MANDATORY_MEMB | RHBZ_READ_INT);
    bz->bi_id = *ret;
    free(ret);
    bz->bi_product = rhbz_bug_read_item("product", xml_bug_response,
                                        RHBZ_MANDATORY_MEMB | RHBZ_READ_STR);
    bz->bi_reporter = rhbz_bug_read_item("reporter", xml_bug_response,
                                         RHBZ_MANDATORY_MEMB | RHBZ_READ_STR);
    bz->bi_status = rhbz_bug_read_item("bug_status", xml_bug_response,
                                       RHBZ_MANDATORY_MEMB | RHBZ_READ_STR);
    bz->bi_resolution = rhbz_bug_read_item("resolution", xml_bug_response,
                                           RHBZ_READ_STR);

    if (strcmp(bz->bi_status, "CLOSED") == 0 && !bz->bi_resolution)
        error_msg_and_die(_("Bug %i is CLOSED, but it has no RESOLUTION"), bz->bi_id);

    ret = (int*)rhbz_bug_read_item("dup_id", xml_bug_response,
                                   RHBZ_READ_INT);
    if (strcmp(bz->bi_status, "CLOSED") == 0
        && strcmp(bz->bi_resolution, "DUPLICATE") == 0
        && !ret)
    {
        error_msg_and_die(_("Bug %i is CLOSED as DUPLICATE, but it has no DUP_ID"),
                            bz->bi_id);
    }

    bz->bi_dup_id = (ret) ? *ret: -1;
    free(ret);

    bz->bi_cc_list = rhbz_bug_cc(xml_bug_response);

    xmlrpc_DECREF(xml_bug_response);

    return bz;
}

/* suppress mail notify by {s:i} (nomail:1) (driven by flag) */
int rhbz_new_bug(struct abrt_xmlrpc *ax, problem_data_t *problem_data,
                 int depend_on_bug)
{
    const char *package      = get_problem_item_content_or_NULL(problem_data,
                                                                FILENAME_PACKAGE);
    const char *component    = get_problem_item_content_or_NULL(problem_data,
                                                                FILENAME_COMPONENT);
    const char *release      = get_problem_item_content_or_NULL(problem_data,
                                                                FILENAME_OS_RELEASE);
    if (!release) /* Old dump dir format compat. Remove in abrt-2.1 */
        release = get_problem_item_content_or_NULL(problem_data, "release");
    const char *arch         = get_problem_item_content_or_NULL(problem_data,
                                                                FILENAME_ARCHITECTURE);
    const char *duphash      = get_problem_item_content_or_NULL(problem_data,
                                                                FILENAME_DUPHASH);
    const char *reason       = get_problem_item_content_or_NULL(problem_data,
                                                                FILENAME_REASON);
    const char *function     = get_problem_item_content_or_NULL(problem_data,
                                                                FILENAME_CRASH_FUNCTION);
    const char *analyzer     = get_problem_item_content_or_NULL(problem_data,
                                                                FILENAME_ANALYZER);
    const char *tainted_str  = get_problem_item_content_or_NULL(problem_data,
                                                                FILENAME_TAINTED);

    struct strbuf *buf_summary = strbuf_new();
    strbuf_append_strf(buf_summary, "[abrt] %s", package);

    if (function != NULL && strlen(function) < 30)
        strbuf_append_strf(buf_summary, ": %s", function);

    if (reason != NULL)
        strbuf_append_strf(buf_summary, ": %s", reason);

    if (tainted_str && analyzer
        && (strcmp(analyzer, "Kerneloops") == 0)
    ) {
        //TODO: fix me; basically it doesn't work as it suppose to work
        //      I will fix it immediately when this patch land into abrt git 
        /*
        unsigned long tainted = xatoi_positive(tainted_str);
        const char *tainted_warning = tainted_string(tainted);
        if (tainted_warning)
            strbuf_append_strf(buf_summary, ": TAINTED %s", tainted_warning);
        */
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
    if (depend_on_bug > -1)
    {
        result = abrt_xmlrpc_call(ax, "Bug.create", "({s:s,s:s,s:s,s:s,s:s,s:s,s:s,s:i})",
                                  "product", product,
                                  "component", component,
                                  "version", version,
                                  "summary", summary,
                                  "description", full_dsc,
                                  "status_whiteboard", status_whiteboard,
                                  "platform", arch,
                                  "dependson", depend_on_bug);
    }
    else
    {
        result = abrt_xmlrpc_call(ax, "Bug.create", "({s:s,s:s,s:s,s:s,s:s,s:s,s:s})",
                                  "product", product,
                                  "component", component,
                                  "version", version,
                                  "summary", summary,
                                  "description", full_dsc,
                                  "status_whiteboard", status_whiteboard,
                                  "platform", arch);
    }
    free(status_whiteboard);
    free(product);
    free(version);
    free(summary);
    free(full_dsc);

    if (!result)
        return -1;

    int *r = rhbz_bug_read_item("id", result, RHBZ_MANDATORY_MEMB | RHBZ_READ_INT);
    xmlrpc_DECREF(result);
    int new_bug_id = *r;
    free(r);

    log(_("New bug id: %i"), new_bug_id);
    return new_bug_id;
}

/* suppress mail notify by {s:i} (nomail:1) (driven by flag) */
int rhbz_attachment(struct abrt_xmlrpc *ax, const char *filename,
                    const char *bug_id, const char *data, int flags)
{
    char *encoded64 = encode_base64(data, strlen(data));
    char *fn = xasprintf("File: %s", filename);
    xmlrpc_value* result;
    int nomail_notify = IS_NOMAIL_NOTIFY(flags);

    result= abrt_xmlrpc_call(ax, "bugzilla.addAttachment", "(s{s:s,s:s,s:s,s:s,s:i})",
                             bug_id,
                             "description", fn,
                             "filename", filename,
                             "contenttype", "text/plain",
                             "data", encoded64,
                             "nomail", nomail_notify);

    free(encoded64);
    free(fn);
    if (!result)
        return -1;

    xmlrpc_DECREF(result);

    return 0;
}

/* suppress mail notify by {s:i} (nomail:1) (driven by flag) */
int rhbz_attachments(struct abrt_xmlrpc *ax, const char *bug_id,
                     problem_data_t *problem_data, int flags)
{
    GHashTableIter iter;
    char *name;
    struct problem_item *value;
    g_hash_table_iter_init(&iter, problem_data);
    while (g_hash_table_iter_next(&iter, (void**)&name, (void**)&value))
    {
        const char *content = value->content;

        // We were special-casing FILENAME_BACKTRACE here, but karel says
        // he can retrieve it in inlined form from comments too.
        if ((value->flags & CD_FLAG_TXT)
         && (strlen(content) > CD_TEXT_ATT_SIZE /*|| (strcmp(name, FILENAME_BACKTRACE) == 0)*/)
        ) {
            /* check if the attachment failed and try it once more  */
            rhbz_attachment(ax, name, bug_id, content, flags);
        }
    }

    return 0;
}

void rhbz_logout(struct abrt_xmlrpc *ax)
{
    xmlrpc_value* result = abrt_xmlrpc_call(ax, "User.logout", "(s)", "");
    if (result)
        xmlrpc_DECREF(result);
}

struct bug_info *rhbz_find_origin_bug_closed_duplicate(struct abrt_xmlrpc *ax,
                                                       struct bug_info *bi)
{
    struct bug_info *bi_tmp = new_bug_info();
    bi_tmp->bi_id = bi->bi_id;
    bi_tmp->bi_dup_id = bi->bi_dup_id;

    for (int ii = 0; ii <= MAX_HOPS; ii++)
    {
        if (ii == MAX_HOPS)
            error_msg_and_die(_("Bugzilla couldn't find parent of bug %d"), bi->bi_id);

        log("Bug %d is a duplicate, using parent bug %d", bi_tmp->bi_id, bi_tmp->bi_dup_id);
        int bug_id = bi_tmp->bi_dup_id;

        free_bug_info(bi_tmp);
        bi_tmp = rhbz_bug_info(ax, bug_id);

        // found a bug which is not CLOSED as DUPLICATE
        if (bi_tmp->bi_dup_id == -1)
            break;
    }

    return bi_tmp;
}

/* suppress mail notify by {s:i} (nomail:1) */
void rhbz_mail_to_cc(struct abrt_xmlrpc *ax, int bug_id, const char *mail, int flags)
{
    xmlrpc_value *result;
    int nomail_notify = IS_NOMAIL_NOTIFY(flags);
    result = abrt_xmlrpc_call(ax, "Bug.update", "({s:i,s:{s:(s),s:i}})",
                              "ids", bug_id, "updates", "add_cc", mail,
                              "nomail", nomail_notify);

    if (result)
        xmlrpc_DECREF(result);
}

void rhbz_add_comment(struct abrt_xmlrpc *ax, int bug_id, const char *comment,
                      int flags)
{
    int private = IS_PRIVATE(flags);
    int nomail_notify = IS_NOMAIL_NOTIFY(flags);

    xmlrpc_value *result;
    result = abrt_xmlrpc_call(ax, "Bug.add_comment", "({s:i,s:s,s:b,s:i})",
                              "id", bug_id, "comment", comment,
                              "private", private, "nomail", nomail_notify);

    if (result)
        xmlrpc_DECREF(result);
}
