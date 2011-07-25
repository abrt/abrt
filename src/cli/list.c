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

#include "abrtlib.h"
#include "builtin-cmd.h"

/* TODO: npajkovs
 *     add --since
 *     add --until
 *     add --pretty=oneline|raw|normal|format="%a %b %c"
 *     add  wildcard e.g. *-2011-04-01-10-* (list all problems in specific day)
 *
 * TODO?: remove base dir from list of crashes? is there a way that same crash can be in
 *       ~/.abrt/spool and /var/spool/abrt? needs more _meditation_.
 */

/* Vector of problems: */
/* problem_data_vector[i] = { "name" = { "content", CD_FLAG_foo_bits } } */
typedef GPtrArray vector_of_problem_data_t;

static inline problem_data_t *get_problem_data(vector_of_problem_data_t *vector, unsigned i)
{
    return (problem_data_t *)g_ptr_array_index(vector, i);
}

static void free_vector_of_problem_data(vector_of_problem_data_t *vector)
{
    if (vector)
        g_ptr_array_free(vector, TRUE);
}

static vector_of_problem_data_t *new_vector_of_problem_data(void)
{
    return g_ptr_array_new_with_free_func((void (*)(void*)) &free_problem_data);
}

static problem_data_t *fill_crash_info(const char *dump_dir_name)
{
    int sv_logmode = logmode;
    logmode = 0; /* suppress EPERM/EACCES errors in opendir */
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ DD_OPEN_READONLY);
    logmode = sv_logmode;

    if (!dd)
        return NULL;

    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
    dd_close(dd);
    add_to_problem_data_ext(problem_data, CD_DUMPDIR, dump_dir_name,
                            CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE + CD_FLAG_LIST);

    return problem_data;
}

static void get_all_crash_infos(vector_of_problem_data_t *retval, const char *dir_name)
{
    VERB1 log("Loading dumps from '%s'", dir_name);

    DIR *dir = opendir(dir_name);
    if (dir != NULL)
    {
        struct dirent *dent;
        while ((dent = readdir(dir)) != NULL)
        {
            if (dot_or_dotdot(dent->d_name))
                continue; /* skip "." and ".." */

            char *dump_dir_name = concat_path_file(dir_name, dent->d_name);

            struct stat statbuf;
            if (stat(dump_dir_name, &statbuf) == 0
                && S_ISDIR(statbuf.st_mode))
            {
                problem_data_t *problem_data = fill_crash_info(dump_dir_name);
                if (problem_data)
                    g_ptr_array_add(retval, problem_data);
            }
            free(dump_dir_name);
        }
        closedir(dir);
    }
}

/** Prints basic information about a crash to stdout. */
static void print_crash(problem_data_t *problem_data, int detailed)
{
    if (!problem_data)
        return;

    char *desc;
    if (detailed)
    {
        int show_multiline = (detailed ? MAKEDESC_SHOW_MULTILINE : 0);
        desc = make_description(problem_data,
                                /*names_to_skip:*/ NULL,
                                /*max_text_size:*/ CD_TEXT_ATT_SIZE,
                                MAKEDESC_SHOW_FILES | show_multiline);
    }
    else
    {
        desc = make_description(problem_data,
                            /*names_to_skip:*/ NULL,
                            /*max_text_size:*/ CD_TEXT_ATT_SIZE,
                            MAKEDESC_SHOW_ONLY_LIST);
    }
    fprintf(stdout, "%s", desc);
    free(desc);
}

/**
 * Prints a list containing "crashes" to stdout.
 * @param include_reported
 *   Do not skip entries marked as already reported.
 */
static void print_crash_list(vector_of_problem_data_t *crash_list, int include_reported,
                             int detailed)
{
    unsigned i;
    for (i = 0; i < crash_list->len; ++i)
    {
        problem_data_t *crash = get_problem_data(crash_list, i);
        if (!include_reported)
        {
            const char *msg = get_problem_item_content_or_NULL(crash, FILENAME_REPORTED_TO);
            if (msg)
                continue;
        }
        print_crash(crash, detailed);
        if (i != crash_list->len - 1)
            printf("\n");
    }
}

int cmd_list(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "\b list [options] [<dump-dir>]..."
        );

    static int opt_full, opt_detailed;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_GROUP(""),
        OPT_BOOL('f', "full"     , &opt_full,      _("List even reported problems")),
        /* deprecate -d option with --pretty=full*/
        OPT_BOOL('d', "detailed" , &opt_detailed,  _("Show detailed report")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);

    GList *D_list = NULL;
    if (optind < argc)
        while (optind < argc)
            D_list = g_list_append(D_list, xstrdup(argv[optind++]));

    load_abrt_conf();
    if (!D_list)
    {
        char *home = getenv("HOME");
        if (home)
            D_list = g_list_append(D_list, concat_path_file(home, ".abrt/spool"));
        D_list = g_list_append(D_list, xstrdup(g_settings_dump_location));
    }
    free_abrt_conf_data();

    VERB2
    {
        log("Base directory");
        for (GList *li = D_list; li; li = li->next)
            log("\t %s", (char *) li->data);
    }

    vector_of_problem_data_t *ci = new_vector_of_problem_data();
    while (D_list)
    {
        char *dir = (char *)D_list->data;
        get_all_crash_infos(ci, dir);
        D_list = g_list_remove(D_list, dir);
        free(dir);
    }
    print_crash_list(ci, opt_full, opt_detailed);
    free_vector_of_problem_data(ci);
    g_list_free(D_list);

    return 0;
}

int cmd_info(int argc, const char **argv)
{
    const char *program_usage_string = _(
        "\b info [options] [<dump-dir>]..."
        );

    static int opt_detailed;
    struct options program_options[] = {
        OPT__VERBOSE(&g_verbose),
        OPT_GROUP(""),
        /* deprecate -d option with --pretty=full*/
        OPT_BOOL('d', "detailed" , &opt_detailed,  _("Show detailed report")),
        OPT_END()
    };

    parse_opts(argc, (char **)argv, program_options, program_usage_string);

    if (optind < argc)
    {
        while (optind < argc)
        {
            const char *dump_dir = argv[optind++];
            problem_data_t *problem = fill_crash_info(dump_dir);
            if (!problem)
            {
                error_msg("no such problem directory '%s'", dump_dir);
                continue;
            }
            print_crash(problem, opt_detailed);
            free_problem_data(problem);
            if (optind - argc)
                printf("\n");
        }
        exit(0);
    }

    show_usage_and_die(program_usage_string, program_options);

    return 0;
}
