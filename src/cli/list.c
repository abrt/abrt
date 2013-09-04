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

#include "libabrt.h"

#include "abrt-cli-core.h"
#include "builtin-cmd.h"

/* TODO: npajkovs
 *     add --pretty=oneline|raw|normal|format="%a %b %c"
 *     add  wildcard e.g. *-2011-04-01-10-* (list all problems in specific day)
 *
 * TODO?: remove base dir from list of crashes? is there a way that same crash can be in
 *       ~/.abrt/spool and /var/tmp/abrt? needs more _meditation_.
 */

static bool isxdigit_str(const char *str)
{
    do
    {
        if (*str < '0' || *str > '9')
            if ((*str|0x20) < 'a' || (*str|0x20) > 'f')
                return false;
        str++;
    } while (*str);
    return true;
}

static char *str2hash(const char *str)
{
    static char result[SHA1_RESULT_LEN*2 + 1];

    char hash_bytes[SHA1_RESULT_LEN];
    sha1_ctx_t sha1ctx;
    sha1_begin(&sha1ctx);
    sha1_hash(&sha1ctx, str, strlen(str));
    sha1_end(&sha1ctx, hash_bytes);
    bin2hex(result, hash_bytes, SHA1_RESULT_LEN)[0] = '\0';
    return result;
}

struct name_resolution_param {
    const char *shortcut;
    unsigned strlen_shortcut;
    char *found_name;
};
static int find_dir_by_hash(struct dump_dir *dd, void *arg)
{
    struct name_resolution_param *param = arg;
    char *h = str2hash(dd->dd_dirname);
    if (strncasecmp(param->shortcut, h, param->strlen_shortcut) == 0)
    {
        if (param->found_name)
            error_msg_and_die(_("'%s' identifies more than one problem directory"), param->shortcut);
        param->found_name = xstrdup(dd->dd_dirname);
    }
    return 0;
}

static problem_data_t *load_problem_data(const char *dump_dir_name)
{
    /* First, try loading by dirname */
    int sv_logmode = logmode;
    logmode = 0; /* suppress EPERM/EACCES errors in opendir */
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ DD_OPEN_READONLY);
    logmode = sv_logmode;

    unsigned name_len = strlen(dump_dir_name);
    /* (git requires at least 5 char hash prefix, we do the same) */
    if (!dd && errno == ENOENT && isxdigit_str(dump_dir_name) && name_len >= 5)
    {
        /* Try loading by dirname hash */
        struct name_resolution_param param = { dump_dir_name, name_len, NULL };
        GList *dir_list = get_problem_storages();
        for (GList *li = dir_list; li; li = li->next)
            for_each_problem_in_dir(li->data, getuid(), find_dir_by_hash, &param);
        if (!param.found_name)
            return NULL;
        dd = dd_opendir(param.found_name, /*flags:*/ DD_OPEN_READONLY);
        free(param.found_name);
    }

    if (!dd)
        return NULL;

    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
    problem_data_add(problem_data, CD_DUMPDIR, dd->dd_dirname,
                            CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE + CD_FLAG_LIST);
    dd_close(dd);

    return problem_data;
}

/** Prints basic information about a crash to stdout. */
static void print_crash(problem_data_t *problem_data, int detailed, int text_size)
{
    if (!problem_data)
        return;

    char *desc;
    if (detailed)
    {
        int show_multiline = (detailed ? MAKEDESC_SHOW_MULTILINE : 0);
        desc = make_description(problem_data,
                                /*names_to_skip:*/ NULL,
                                /*max_text_size:*/ text_size,
                                MAKEDESC_SHOW_FILES | show_multiline);
    }
    else
    {
        desc = make_description(problem_data,
                            /*names_to_skip:*/ NULL,
                            /*max_text_size:*/ text_size,
                            MAKEDESC_SHOW_ONLY_LIST);
    }
    fputs(desc, stdout);
    free(desc);
}

/**
 * Prints a list containing "crashes" to stdout.
 * @param include_reported
 *   Do not skip entries marked as already reported.
 */
static void print_crash_list(vector_of_problem_data_t *crash_list, int detailed, int include_reported, long since, long until, int text_size)
{
    unsigned i;
    for (i = 0; i < crash_list->len; ++i)
    {
        problem_data_t *crash = get_problem_data(crash_list, i);
        if (!include_reported)
        {
            if (problem_data_get_content_or_NULL(crash, FILENAME_REPORTED_TO))
                continue;
        }
        if (since || until)
        {
            char *s = problem_data_get_content_or_NULL(crash, FILENAME_LAST_OCCURRENCE);
            long val = s ? atol(s) : 0;
            if (since && val < since)
                continue;
            if (until && val > until)
                continue;
        }

        struct problem_item *item = g_hash_table_lookup(crash, CD_DUMPDIR);
        if (item)
            printf("@%s\n", str2hash(item->content));
        print_crash(crash, detailed, text_size);
        if (i != crash_list->len - 1)
            printf("\n");
    }
}

int cmd_list(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& list [options] [DIR]..."
        );

    int opt_full = 0;
    int opt_detailed = 0;
    int opt_since = 0;
    int opt_until = 0;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL('f', "full"     , &opt_full,      _("List reported problems too")),
        /* deprecate -d option with --pretty=full*/
        OPT_BOOL('d', "detailed" , &opt_detailed,  _("Show detailed report")),
        OPT_INTEGER('s', "since" , &opt_since,  _("List only the problems more recent than specified timestamp")),
        OPT_INTEGER('u', "until" , &opt_until,  _("List only the problems older than specified timestamp")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);
    argv += optind;

    GList *D_list = NULL;
    while (*argv)
        D_list = g_list_append(D_list, xstrdup(*argv++));
    if (!D_list)
        D_list = get_problem_storages();

    vector_of_problem_data_t *ci = fetch_crash_infos(D_list);

    g_ptr_array_sort_with_data(ci, &cmp_problem_data, (char *) FILENAME_LAST_OCCURRENCE);

    print_crash_list(ci, opt_detailed, opt_full, opt_since, opt_until, CD_TEXT_ATT_SIZE_BZ);
    free_vector_of_problem_data(ci);
    list_free_with_free(D_list);

    return 0;
}

int cmd_info(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "& info [options] DIR..."
        );

    int opt_detailed = 0;
    int text_size = CD_TEXT_ATT_SIZE_BZ;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        /* deprecate -d option with --pretty=full*/
        OPT_BOOL(   'd', "detailed" , &opt_detailed, _("Show detailed report")),
        OPT_INTEGER('s', "size",      &text_size,    _("Text larger than this will be shown abridged")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);
    argv += optind;

    if (!argv[0])
        show_usage_and_die(program_usage_string, program_options);

    if (text_size <= 0)
        text_size = INT_MAX;

    int errs = 0;
    while (*argv)
    {
        const char *dump_dir = *argv++;
        problem_data_t *problem = load_problem_data(dump_dir);
        if (!problem)
        {
            error_msg(_("No such problem directory '%s'"), dump_dir);
            errs++;
            continue;
        }

        print_crash(problem, opt_detailed, text_size);
        problem_data_free(problem);
        if (*argv)
            printf("\n");
    }

    return errs;
}
