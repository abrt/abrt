/*
    Copyright (C) 2011  ABRT Team
    Copyright (C) 2011  RedHat inc.

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

#include <json.h>
#include <rpm/rpmts.h>
#include <rpm/rpmcli.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmpgp.h>
#include <hawkey/util.h>

#include <libreport/internal_libreport.h>
#include <libreport/libreport_curl.h>
#include <libreport/client.h>

//699198,705037,705036

/* bodhi returns json structure

{
 "rows_per_page": 20,
    "total": 1,
    "chrome": true,
    "display_user": true,
    "pages": 1,
    "updates": [
       {
        "close_bugs": true,
        "old_updateid": "FEDORA-2015-13720",
        "pushed": true,
        "require_testcases": false,
        "critpath": false,
        "date_approved": null,
        "stable_karma": null,
        "date_pushed": "2015-08-19 04:49:00",
        "requirements": null,
        "severity": "unspecified",
        "title": "hwloc-1.11.0-3.fc22",
        "suggest": "unspecified",
        "require_bugs": false,
        "comments": [
           {
            "bug_feedback": [],
            "user_id": 91,
            "text": "This update has been submitted for testing by jhladky. ",
            "testcase_feedback": [],
            "karma_critpath": 0,
            "update": 21885,
            "update_id": 21885,
            "karma": 0,
            "anonymous": false,
            "timestamp": "2015-08-18 13:38:35",
            "id": 166016,
            "user": {"stacks": [],
                "name": "bodhi",
                "avatar": "https://apps.fedoraproject.org/img/icons/bodhi-24.png"}
           },
           {
           ...
           }
        ],
        "updateid": "FEDORA-2015-13720",
        "cves": [],
        "type": "bugfix",
        "status": "testing",
        "date_submitted": "2015-08-18 13:37:26",
        "unstable_karma": null,
        "submitter": "jhladky",
        "user":
           {
            "stacks": [],
            "buildroot_overrides": [],
            "name": "jhladky",
            "avatar": "https://seccdn.libravatar.org/avatar/b838b78fcf707a13cdaeb1c846d514e614d617cbf2c106718e71cb397607f59b?s=24&d=retro"
           },
        "locked": false,
        "builds": [{"override": null,
            "nvr": "hwloc-1.11.0-3.fc22"}],
        "date_modified": null,
        "test_cases": [],
        "notes": "Fix for BZ1253977",
        "request": null,
        "bugs": [
           {
            "bug_id": 1253977,
            "security": false,
            "feedback": [],
            "parent": false,
            "title": "conflict between hwloc-libs-1.11.0-1.fc22.i686 and hwloc-libs-1.11.0-1.fc22.x86_64"
           }
        ],
        "alias": "FEDORA-2015-13720",
        "karma": 0,
        "release":
           {
            "dist_tag": "f22",
            "name": "F22",
            "testing_tag": "f22-updates-testing",
            "pending_stable_tag": "f22-updates-pending",
            "long_name": "Fedora 22",
            "state": "current",
            "version": "22",
            "override_tag": "f22-override",
            "branch": "f22",
            "id_prefix": "FEDORA",
            "pending_testing_tag": "f22-updates-testing-pending",
            "stable_tag": "f22-updates",
            "candidate_tag": "f22-updates-candidate"
           }
       }
    ],
    "display_request": true,
    "page": 1
}
*/

static const char *bodhi_url = "https://bodhi.fedoraproject.org/updates";

struct bodhi {
    char *nvr;
#if 0
    char *date_pushed;
    char *status;
    char *dist_tag;

    GList *bz_ids;
#endif
};

enum {
    BODHI_READ_STR,
    BODHI_READ_INT,
    BODHI_READ_JSON_OBJ,
};

static void free_bodhi_item(struct bodhi *b)
{
    if (!b)
        return;

    free(b->nvr);

#if 0
    list_free_with_free(b->bz_ids);
    free(b->date_pushed);
    free(b->status);
    free(b->dist_tag);
#endif

    free(b);
}

static void bodhi_read_value(json_object *json, const char *item_name,
                             void *value, int flags)
{
    json_object *j = NULL;
    if (!json_object_object_get_ex(json, item_name, &j))
    {
        error_msg("'%s' section is not available", item_name);
        return;
    }

    switch (flags) {
    case BODHI_READ_INT:
        *(int *) value = json_object_get_int(j);
        break;
    case BODHI_READ_STR:
        *(char **) value = (char *) strtrimch(xstrdup(json_object_to_json_string(j)), '"');
        break;
    case BODHI_READ_JSON_OBJ:
        *(json_object **) value = (json_object *) j;
        break;
    };
}

#if 0
static void print_bodhi(struct bodhi *b)
{
    for (GList *l = b->nvr; l; l = l->next)
        printf("'%s' ", (char *)l->data);

    for (GList *l = b->name; l; l = l->next)
        printf("'%s' ", (char *)l->data);

    if (b->date_pushed)
        printf(" '%s'", b->date_pushed);

    if (b->status)
        printf(" '%s'", b->status);

    if (b->dist_tag)
        printf(" '%s'", b->dist_tag);

    printf(" %i", b->karma);


/*
    for (GList *li = b->bz_ids; li; li = li->next)
        printf(" %i", *(int*) li->data);
*/
    puts("");
}
#endif

static GHashTable *bodhi_parse_json(json_object *json, const char *release)
{

    int num_items = 0;
    bodhi_read_value(json, "total", &num_items, BODHI_READ_INT);
    if (num_items <= 0)
        return NULL;

    json_object *updates = NULL;
    bodhi_read_value(json, "updates", &updates, BODHI_READ_JSON_OBJ);
    if (!updates)
        return NULL;

    int updates_len = json_object_array_length(updates);

    GHashTable *bodhi_table = g_hash_table_new_full(g_str_hash, g_str_equal, free,
                                                    (GDestroyNotify) free_bodhi_item);
    for (int i = 0; i < updates_len; ++i)
    {
        json_object *updates_item = json_object_array_get_idx(updates, i);

        /* some of item are null */
        if (!updates_item)
            continue;

        json_object *builds_item = NULL;
        bodhi_read_value(updates_item, "builds", &builds_item, BODHI_READ_JSON_OBJ);
        if (!builds_item) /* broken json */
            continue;

        int karma, unstable_karma;
        bodhi_read_value(updates_item, "karma", &karma, BODHI_READ_INT);
        bodhi_read_value(updates_item, "unstable_karma", &unstable_karma, BODHI_READ_INT);
        if (karma <= unstable_karma)
            continue;

        struct bodhi *b = NULL;
        int builds_len = json_object_array_length(builds_item);
        for (int k = 0; k < builds_len; ++k)
        {
            b = xzalloc(sizeof(struct bodhi));

            char *name = NULL;
            long ign_e;
            char *ign_v, *ign_r, *ign_a;

            json_object *build = json_object_array_get_idx(builds_item, k);

            bodhi_read_value(build, "nvr", &b->nvr, BODHI_READ_STR);

            if (hy_split_nevra(b->nvr, &name, &ign_e, &ign_v, &ign_r, &ign_a))
                error_msg_and_die("hawkey failed to parse '%s'", b->nvr);

            free(ign_v);
            free(ign_r);
            free(ign_a);

            struct bodhi *bodhi_tbl_item = g_hash_table_lookup(bodhi_table, name);
            if (bodhi_tbl_item && rpmvercmp(bodhi_tbl_item->nvr, b->nvr) > 0)
            {
                free_bodhi_item(b);
                continue;
            }
            g_hash_table_replace(bodhi_table, name, b);
        }

#if 0
        bodhi_read_value(updates_item, "date_pushed", &b->date_pushed, BODHI_READ_STR);
        bodhi_read_value(updates_item, "status", &b->status, BODHI_READ_STR);

        json_object *release_item = NULL;
        bodhi_read_value(updates_item, "release", &release_item, BODHI_READ_JSON_OBJ);
        if (release_item)
            bodhi_read_value(release_item, "dist_tag", &b->dist_tag, BODHI_READ_STR);

        json_object *bugs = NULL;
        bodhi_read_value(updates_item, "bugs", &release_item, BODHI_READ_JSON_OBJ);
        if (bugs)
        {
            for (int j = 0; j < json_object_array_length(bugs); ++j)
            {
                int *bz_id = xmalloc(sizeof(int));
                json_object *bug_item = json_object_array_get_idx(bugs, j);
                bodhi_read_value(bug_item, "bz_id", bz_id, BODHI_READ_INT);
                b->bz_ids = g_list_append(b->bz_ids, bz_id);
            }
        }
#endif
    }

    return bodhi_table;
}

static GHashTable *bodhi_query_list(const char *query, const char *release)
{
    char *bodhi_url_bugs = xasprintf("%s/?%s", bodhi_url, query);

    post_state_t *post_state = new_post_state(POST_WANT_BODY
                                              | POST_WANT_SSL_VERIFY
                                              | POST_WANT_ERROR_MSG);

    const char *headers[] = {
        "Accept: application/json",
        NULL
    };

    get(post_state, bodhi_url_bugs, "application/x-www-form-urlencoded",
                     headers);

    if (post_state->http_resp_code != 200)
    {
        char *errmsg = post_state->curl_error_msg;
        if (errmsg && errmsg[0])
            error_msg_and_die("%s '%s'", errmsg, bodhi_url_bugs);
    }
    free(bodhi_url_bugs);

//    log("%s", post_state->body);

    json_object *json = json_tokener_parse(post_state->body);
    if (is_error(json))
        error_msg_and_die("fatal: unable parse response from bodhi server");

    GHashTable *bodhi_table = bodhi_parse_json(json, release);
    json_object_put(json);
    free_post_state(post_state);

    return bodhi_table;
}

static char *rpm_get_nvr_by_pkg_name(const char *pkg_name)
{
    int status = rpmReadConfigFiles((const char *) NULL, (const char *) NULL);
    if (status)
        error_msg_and_die("error reading RPM rc files");

    char *nvr = NULL;

    rpmts ts = rpmtsCreate();
    rpmdbMatchIterator iter = rpmtsInitIterator(ts, RPMTAG_NAME, pkg_name, 0);
    Header header = rpmdbNextIterator(iter);

    if (!header)
        goto error;

    const char *errmsg = NULL;
    nvr = headerFormat(header, "%{name}-%{version}-%{release}", &errmsg);

    if (!nvr && errmsg)
        error_msg("cannot get nvr. reason: %s", errmsg);

error:
    rpmdbFreeIterator(iter);
    rpmtsFree(ts);

    rpmFreeRpmrc();
    rpmFreeCrypto();
    rpmFreeMacros(NULL);

    return nvr;
}

int main(int argc, char **argv)
{
    abrt_init(argv);
    enum {
        OPT_v = 1 << 0,
        OPT_d = 1 << 1,
        OPT_g = 1 << 2,
        OPT_b = 1 << 3,
        OPT_u = 1 << 4,
        OPT_r = 1 << 5,
    };

    const char *bugs = NULL, *release = NULL, *dump_dir_path = ".";
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT__DUMP_DIR(&dump_dir_path),
        OPT_GROUP(""),
        OPT_STRING('b', "bugs", &bugs, "ID1[,ID2,...]" , _("List of bug ids")),
        OPT_STRING('u', "url", &bodhi_url, "URL", _("Specify a bodhi server url")),
        OPT_OPTSTRING('r', "release", &release, "RELEASE", _("Specify a release")),
        OPT_END()
    };

    const char *program_usage_string = _(
        "& [-v] [-r[RELEASE]] (-b ID1[,ID2,...] | PKG-NAME) [PKG-NAME]... \n"
        "\n"
        "Search for updates on bodhi server"
    );

    unsigned opts =  parse_opts(argc, argv, program_options, program_usage_string);

    if (!bugs && !argv[optind])
        show_usage_and_die(program_usage_string, program_options);

    struct strbuf *query = strbuf_new();
    if (bugs)
        query = strbuf_append_strf(query, "bugs=%s&", bugs);

    if (opts & OPT_r)
    {
        if (release)
        {
            query = strbuf_append_strf(query, "releases=%s&", release);
        }
        else
        {
            struct dump_dir *dd = dd_opendir(dump_dir_path, DD_OPEN_READONLY);
            if (!dd)
                xfunc_die();

            problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
            dd_close(dd);
            if (!problem_data)
                xfunc_die(); /* create_problem_data_for_reporting already emitted error msg */

            char *product, *version;
            map_string_t *osinfo = new_map_string();
            problem_data_get_osinfo(problem_data, osinfo);
            parse_osinfo_for_rhts(osinfo, &product, &version);
            query = strbuf_append_strf(query, "releases=f%s&", version);
            free(product);
            free(version);
            free_map_string(osinfo);
        }
    }

    if (argv[optind])
    {
        char *escaped = g_uri_escape_string(argv[optind], NULL, 0);
        query = strbuf_append_strf(query, "packages=%s&", escaped);
        free(escaped);
    }

    if (query->buf[query->len - 1] == '&')
        query->buf[query->len - 1] = '\0';

    log(_("Searching for updates"));
    GHashTable *update_hash_tbl = bodhi_query_list(query->buf, release);
    strbuf_free(query);

    if (!update_hash_tbl || !g_hash_table_size(update_hash_tbl))
    {
        log(_("No updates for this package found"));
        /*if (update_hash_tbl) g_hash_table_unref(update_hash_tbl);*/
        return 0;
    }

    GHashTableIter iter;
    char *name;
    struct bodhi *b;
    struct strbuf *q = strbuf_new();
    g_hash_table_iter_init(&iter, update_hash_tbl);
    while (g_hash_table_iter_next(&iter, (void **) &name, (void **) &b))
    {
        char *installed_pkg_nvr = rpm_get_nvr_by_pkg_name(name);
        if (installed_pkg_nvr && rpmvercmp(installed_pkg_nvr, b->nvr) >= 0)
        {
            log_info("Update %s is older or same as local version %s, skipping", b->nvr, installed_pkg_nvr);
            free(installed_pkg_nvr);
            continue;
        }
        free(installed_pkg_nvr);

        strbuf_append_strf(q, " %s", b->nvr);
    }

    /*g_hash_table_unref(update_hash_tbl);*/

    if (!q->len)
    {
        /*strbuf_free(q);*/
        log(_("Local version of the package is newer than available updates"));
        return 0;
    }

    /* Message is split into text and command in order to make
     * translator's job easier
     */

    /* We suggest the command which is most likely to exist on user's system,
     * and which is familiar to the largest population of users.
     * There are other tools (pkcon et al) which might be somewhat more
     * convenient (for example, they might be usable from non-root), but they
     * might be not present on the system, may evolve or be superseded,
     * while yum is unlikely to do so.
     */
    strbuf_prepend_str(q, "yum update --enablerepo=fedora --enablerepo=updates-testing");

    char *msg = xasprintf(_("An update exists which might fix your problem. "
                "You can install it by running: %s. "
                "Do you want to continue with reporting the bug?"),
                q->buf
    );
    /*strbuf_free(q);*/

    return ask_yes_no(msg) ? 0 : EXIT_STOP_EVENT_RUN;
}
