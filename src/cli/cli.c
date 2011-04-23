/*
    Copyright (C) 2009, 2010  Red Hat, Inc.

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
#if HAVE_LOCALE_H
# include <locale.h>
#endif
#include <getopt.h>
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "report.h"

static problem_data_t *FillCrashInfo(const char *dump_dir_name)
{
    int sv_logmode = logmode;
    logmode = 0; /* suppress EPERM/EACCES errors in opendir */
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ DD_OPEN_READONLY);
    logmode = sv_logmode;

    if (!dd)
        return NULL;

    problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
    dd_close(dd);
    add_to_problem_data_ext(problem_data, CD_DUMPDIR, dump_dir_name, CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

    return problem_data;
}

static void GetCrashInfos(vector_of_problem_data_t *retval, const char *dir_name)
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
             && S_ISDIR(statbuf.st_mode)
            ) {
                problem_data_t *problem_data = FillCrashInfo(dump_dir_name);
                if (problem_data)
                    g_ptr_array_add(retval, problem_data);
            }
            free(dump_dir_name);
        }
        closedir(dir);
    }
}

/** Prints basic information about a crash to stdout. */
static void print_crash(problem_data_t *problem_data)
{
    struct problem_item *item = g_hash_table_lookup(problem_data, CD_DUMPDIR);
    if (item)
        printf("\tDirectory   : %s\n", item->content);
    GList *list = g_hash_table_get_keys(problem_data);
    GList *l = list = g_list_sort(list, (GCompareFunc)strcmp);
    while (l)
    {
        const char *key = l->data;
        item = g_hash_table_lookup(problem_data, key);
        if (item && (item->flags & CD_FLAG_LIST) && !strchr(item->content, '\n'))
        {
            char *formatted = format_problem_item(item);
            printf("\t%-12s: %s\n", key, formatted ? formatted : item->content);
            free(formatted);
        }
        l = l->next;
    }
    g_list_free(list);
}

/**
 * Prints a list containing "crashes" to stdout.
 * @param include_reported
 *   Do not skip entries marked as already reported.
 */
static void print_crash_list(vector_of_problem_data_t *crash_list, bool include_reported)
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

        printf("%u.\n", i);
        print_crash(crash);
    }
}

/**
 * Prints full information about a crash
 */
static void print_crash_info(problem_data_t *problem_data, bool show_multiline)
{
    struct problem_item *item = g_hash_table_lookup(problem_data, CD_DUMPDIR);
    if (item)
        printf("%-16s: %s\n", "Directory", item->content);

    GList *list = g_hash_table_get_keys(problem_data);
    GList *l = list = g_list_sort(list, (GCompareFunc)strcmp);
    bool multi_line = 0;
    while (l)
    {
        const char *key = l->data;
        if (strcmp(key, CD_DUMPDIR) != 0)
        {
            item = g_hash_table_lookup(problem_data, key);
            if (item)
            {
                char *formatted = format_problem_item(item);
                char *output = formatted ? formatted : item->content;
                char *last_eol = strrchr(output, '\n');
                if (show_multiline || !last_eol)
                {
                    /* prev value was multi-line, or this value is multi-line? */
                    if (multi_line || last_eol)
                        printf("\n");
                    printf("%-16s:%c%s", key, (last_eol ? '\n' : ' '), output);
                    if (!last_eol || last_eol[1] != '\0')
                        printf("\n"); /* go to next line only if necessary */
                    multi_line = (last_eol != NULL);
                }
                free(formatted);
            }
        }
        l = l->next;
    }
    g_list_free(list);
}

/* Program options */
enum
{
    OPT_GET_LIST,
    OPT_REPORT,
    OPT_DELETE,
    OPT_INFO
};

/**
 * Long options.
 * Do not use the has_arg field. Arguments are handled after parsing all options.
 * The reason is that we want to use all the following combinations:
 *   --report ID
 *   --report ID --always
 *   --report --always ID
 */
static const struct option longopts[] =
{
    /* name, has_arg, flag, val */
    { "help"     , no_argument, NULL, '?' },
    { "verbose"  , no_argument, NULL, 'v' },
    { "version"  , no_argument, NULL, 'V' },
    { "list"     , no_argument, NULL, 'l' },
    { "full"     , no_argument, NULL, 'f' },
    { "always"   , no_argument, NULL, 'y' },
    { "report"   , no_argument, NULL, 'r' },
    { "delete"   , no_argument, NULL, 'd' },
    { "info"     , no_argument, NULL, 'i' },
    { 0, 0, 0, 0 } /* prevents crashes for unknown options*/
};

/* Gets the program name from the first command line argument. */
static const char *progname(const char *argv0)
{
    const char* name = strrchr(argv0, '/');
    if (name)
        return ++name;
    return argv0;
}

/**
 * Prints abrt-cli version and some help text.
 * Then exits the program with return value 1.
 */
static void print_usage_and_die(char *argv0)
{
    const char *name = progname(argv0);
    printf("%s "VERSION"\n\n", name);

    /* Message has embedded tabs. */
    printf(_(
        "Usage: %s -l[f] [-D BASE_DIR]...]\n"
	"   or: %s -r[y] CRASH_DIR\n"
	"   or: %s -i[b] CRASH_DIR\n"
	"   or: %s -d CRASH_DIR\n"
        "\n"
        "	-l, --list		List not yet reported problems\n"
        "	  -f, --full		List all problems\n"
        "	-D BASE_DIR		Directory to list problems from\n"
        "				(default: -D $HOME/.abrt/spool -D %s)\n"
        "\n"
        "	-r, --report		Send a report about CRASH_DIR\n"
        "	  -y, --always		...without editing and asking\n"
        "	-i, --info		Print detailed information about CRASH_DIR\n"
        "	  -f, --full		...including multi-line entries\n"
        "				Note: -if will run analyzers\n"
	"				(if this CRASH_DIR have defined analyzers)\n"
        "	-d, --delete		Remove CRASH_DIR\n"
        "\n"
        "	-V, --version		Display version and exit\n"
        "	-v, --verbose		Be verbose\n"
        ),
        name, name, name, name,
        DEBUG_DUMPS_DIR
    );
    exit(1);
}

int main(int argc, char** argv)
{
    GList *D_list = NULL;
    char *dump_dir_name = NULL;
    int op = -1;
    bool full = false;
    bool always = false;

    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    while (1)
    {
        /* Do not use colons, arguments are handled after parsing all options. */
        int c = getopt_long(argc, argv, "?Vvrdlfyib", longopts, NULL);

#define SET_OP(newop)                                                        \
        do {                                                                 \
          if (op != -1 && op != newop)                                       \
            error_msg_and_die(_("You must specify exactly one operation"));  \
          op = newop;                                                        \
        } while (0)

        switch (c)
        {
        case -1: goto end_of_arg_parsing;
        case 'r': SET_OP(OPT_REPORT);   break;
        case 'd': SET_OP(OPT_DELETE);   break;
        case 'l': SET_OP(OPT_GET_LIST); break;
        case 'i': SET_OP(OPT_INFO);     break;
        case 'f': full = true;          break;
        case 'y': always = true;        break;
        case 'v': g_verbose++;          break;
        case 'D':
            D_list = g_list_append(D_list, optarg);
            break;
        case 'V':
            printf("%s "VERSION"\n", progname(argv[0]));
            return 0;
        case '?':
        default: /* some error */
            print_usage_and_die(argv[0]); /* exits app */
        }
#undef SET_OP
    }
 end_of_arg_parsing: ;

    if (!D_list)
    {
        char *home = getenv("HOME");
        if (home)
            D_list = g_list_append(D_list, concat_path_file(home, ".abrt/spool"));
        D_list = g_list_append(D_list, (void*)DEBUG_DUMPS_DIR);
    }

    /* Handle option arguments. */
    argc -= optind;
    switch (argc)
    {
    case 0:
        if (op == OPT_REPORT || op == OPT_DELETE || op == OPT_INFO)
            print_usage_and_die(argv[0]);
        break;
    case 1:
        if (op != OPT_REPORT && op != OPT_DELETE && op != OPT_INFO)
            print_usage_and_die(argv[0]);
        dump_dir_name = argv[optind];
        break;
    default:
        print_usage_and_die(argv[0]);
    }

    /* Check if we have an operation.
     * Limit --full and --always to certain operations.
     */
    if ((always && op != OPT_REPORT) || op == -1)
    {
        print_usage_and_die(argv[0]);
    }

    /* Get settings */
    load_event_config_data();

    /* Do the selected operation. */
    int exitcode = 0;
    switch (op)
    {
        case OPT_GET_LIST:
        {
            vector_of_problem_data_t *ci = new_vector_of_problem_data();
            while (D_list)
            {
                char *dir = (char *)D_list->data;
                GetCrashInfos(ci, dir);
                D_list = g_list_remove(D_list, dir);
            }
            print_crash_list(ci, full);
            free_vector_of_problem_data(ci);
            break;
        }
        case OPT_REPORT:
        {
            struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
            if (!dd)
                break;
            int readonly = !dd->locked;
            dd_close(dd);
            if (readonly)
            {
                log("'%s' is not writable", dump_dir_name);
                /* D_list can't be NULL here */
                struct dump_dir *dd_copy = steal_directory((char *)D_list->data, dump_dir_name);
                if (dd_copy)
                {
                    delete_dump_dir_possibly_using_abrtd(dump_dir_name);
                    dump_dir_name = xstrdup(dd_copy->dd_dirname);
                    dd_close(dd_copy);
                }
            }

            exitcode = report(dump_dir_name, (always ? CLI_REPORT_BATCH : 0));
            if (exitcode == -1)
                error_msg_and_die("Crash '%s' not found", dump_dir_name);
            break;
        }
        case OPT_DELETE:
        {
            exitcode = delete_dump_dir_possibly_using_abrtd(dump_dir_name);
            break;
        }
        case OPT_INFO:
        {
            /* Load problem_data from dump dir */
            struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
            if (!dd)
                return -1;

            char *analyze_events_as_lines = list_possible_events(dd, NULL, "analyze");

            if (full && analyze_events_as_lines && *analyze_events_as_lines)
            {
                dd_close(dd);

                GList *list_analyze_events = str_to_glist(analyze_events_as_lines, '\n');
                free(analyze_events_as_lines);

                char *event = select_event_option(list_analyze_events);
                list_free_with_free(list_analyze_events);

                int analyzer_result = run_analyze_event(dump_dir_name, event);
                free(event);

                if (analyzer_result != 0)
                    return 1;

                /* Reload problem_data from (possibly updated by analyze) dump dir */
                dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
                if (!dd)
                    return -1;
            } else
                free(analyze_events_as_lines);

            problem_data_t *problem_data = create_problem_data_from_dump_dir(dd);
            dd_close(dd);

            add_to_problem_data_ext(problem_data, CD_DUMPDIR, dump_dir_name,
                                  CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

            print_crash_info(problem_data, full);
            free_problem_data(problem_data);

            break;
        }
    }

    return exitcode;
}
