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
    add_to_problem_data_ext(problem_data, CD_DUMPDIR, dump_dir_name, CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE + CD_FLAG_LIST);

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
    char* desc = make_description(
                problem_data,
                /*names_to_skip:*/ NULL,
                /*max_text_size:*/ CD_TEXT_ATT_SIZE,
                MAKEDESC_SHOW_ONLY_LIST
    );
    fputs(desc, stdout);
    free(desc);
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
    char* desc = make_description(
                problem_data,
                /*names_to_skip:*/ NULL,
                /*max_text_size:*/ CD_TEXT_ATT_SIZE,
                MAKEDESC_SHOW_FILES | (show_multiline ? MAKEDESC_SHOW_MULTILINE : 0)
    );
    fputs(desc, stdout);
    free(desc);
}

static char *do_log(char *log_line, void *param)
{
    log("%s", log_line);
    return log_line;
}

int main(int argc, char** argv)
{
    abrt_init(argv);

    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    GList *D_list = NULL;
    const char *event_name = NULL;
    const char *pfx = "";

    /* Can't keep these strings/structs static: _() doesn't support that */
    const char *program_usage_string = _(
        "\b [-vsp] -l[f] [-D BASE_DIR]...\n"
        "or: \b [-vsp] -i[f] DUMP_DIR\n"
        "or: \b [-vsp] -L[PREFIX] [DUMP_DIR]\n"
        "or: \b [-vsp] -e EVENT DUMP_DIR\n"
        "or: \b [-vsp] -a[y] DUMP_DIR\n"
        "or: \b [-vsp] -r[y] DUMP_DIR\n"
        "or: \b [-vsp] -d DUMP_DIR"
    );
    enum {
        OPT_list         = 1 << 0,
        OPT_D            = 1 << 1,
        OPT_info         = 1 << 2,
        OPT_list_events  = 1 << 3,
        OPT_run_event    = 1 << 4,
        OPT_analyze      = 1 << 5,
        OPT_report       = 1 << 6,
        OPT_delete       = 1 << 7,
        OPT_version      = 1 << 8,
        OPTMASK_op       = OPT_list|OPT_info|OPT_list_events|OPT_run_event|OPT_analyze|OPT_report|OPT_delete|OPT_version,
        OPTMASK_need_arg = OPT_info|OPT_run_event|OPT_analyze|OPT_report|OPT_delete,
        OPT_f            = 1 << 9,
        OPT_y            = 1 << 10,
        OPT_v            = 1 << 11,
        OPT_s            = 1 << 12,
        OPT_p            = 1 << 13,
    };
    /* Keep enum above and order of options below in sync! */
    struct options program_options[] = {
        /*      short_name long_name  value    parameter_name  help */
        OPT_BOOL(     'l', "list"   , NULL,                    _("List not yet reported problems, or all with -f")),
        OPT_LIST(     'D', NULL     , &D_list, "BASE_DIR",     _("Directory to list problems from (default: -D $HOME/.abrt/spool -D "DEBUG_DUMPS_DIR")")),
        OPT_BOOL(     'i', "info"   , NULL,                    _("Print information about DUMP_DIR (detailed with -f)")),
        OPT_OPTSTRING('L', NULL     , &pfx, "PREFIX",          _("List possible events [which start with PREFIX]")),
        OPT_STRING(   'e', NULL     , &event_name, "EVENT",    _("Run EVENT on DUMP_DIR")),
        OPT_BOOL(     'a', "analyze", NULL,                    _("Run analyze event(s) on DUMP_DIR")),
        OPT_BOOL(     'r', "report" , NULL,                    _("Send a report about DUMP_DIR")),
        OPT_BOOL(     'd', "delete" , NULL,                    _("Remove DUMP_DIR")),
        OPT_BOOL(     'V', "version", NULL,                    _("Display version and exit")),
        OPT_BOOL(     'f', "full"   , NULL,                    _("Full listing")),
        OPT_BOOL(     'y', "always" , NULL,                    _("Noninteractive: don't ask questions, assume 'yes'")),
        OPT__VERBOSE(&g_verbose),
        OPT_BOOL(     's', NULL     , NULL,                    _("Log to syslog")),
        OPT_BOOL(     'p', NULL     , NULL,                    _("Add program names to log")),
        OPT_END()
    };
    unsigned opts = parse_opts(argc, argv, program_options, program_usage_string);
    unsigned op = (opts & OPTMASK_op);
    if (!op || ((op-1) & op))
        /* "You must specify exactly one operation" */
        show_usage_and_die(program_usage_string, program_options);
    argv += optind;
    argc -= optind;
    if (argc > 1
        /* dont_need_arg == have_arg? bad in both cases:
         * TRUE == TRUE (dont need arg but have) or
         * FALSE == FALSE (need arg but havent).
         * OPT_list_events is an exception, it can be used in both cases.
         */
     || ((op != OPT_list_events) && (!(opts & OPTMASK_need_arg) == argc))
    ) {
        show_usage_and_die(program_usage_string, program_options);
    }

    if (op == OPT_version)
    {
        printf("%s "VERSION"\n", g_progname);
        return 0;
    }

    export_abrt_envvars(opts & OPT_p);
    if (opts & OPT_s)
    {
        openlog(msg_prefix, 0, LOG_DAEMON);
        logmode = LOGMODE_SYSLOG;
    }

    char *dump_dir_name = argv[0];
    bool full = (opts & OPT_f);
    bool always = (opts & OPT_y);

    if (!D_list)
    {
        char *home = getenv("HOME");
        if (home)
            D_list = g_list_append(D_list, concat_path_file(home, ".abrt/spool"));
        D_list = g_list_append(D_list, (void*)DEBUG_DUMPS_DIR);
    }

    /* Get settings */
    load_event_config_data();

    /* Do the selected operation. */
    int exitcode = 0;
    switch (op)
    {
        case OPT_list:
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
        case OPT_list_events: /* -L[PREFIX] */
        {
            /* Note that dump_dir_name may be NULL here, it means "show all
             * possible events regardless of dir"
             */
            char *events = list_possible_events(NULL, dump_dir_name, pfx);
            if (!events)
                return 1; /* error msg is already logged */
            fputs(events, stdout);
            free(events);
            break;
        }
        case OPT_run_event: /* -e EVENT: run event */
        {
            struct run_event_state *run_state = new_run_event_state();
            run_state->logging_callback = do_log;
            int r = run_event_on_dir_name(run_state, dump_dir_name, event_name);
            if (r == 0 && run_state->children_count == 0)
                error_msg_and_die("No actions are found for event '%s'", event_name);
            free_run_event_state(run_state);
            break;
        }
        case OPT_analyze:
        {
            /* Load problem_data from dump dir */
            struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
            if (!dd)
                return 1;
            char *analyze_events_as_lines = list_possible_events(dd, NULL, "analyze");
            dd_close(dd);

            if (analyze_events_as_lines && *analyze_events_as_lines)
            {
                GList *list_analyze_events = str_to_glist(analyze_events_as_lines, '\n');
                char *event = select_event_option(list_analyze_events);
                list_free_with_free(list_analyze_events);
                exitcode = run_analyze_event(dump_dir_name, event);
                free(event);
            }
            free(analyze_events_as_lines);
            break;
        }
        case OPT_report:
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
        case OPT_delete:
        {
            exitcode = delete_dump_dir_possibly_using_abrtd(dump_dir_name);
            break;
        }
        case OPT_info:
        {
            /* Load problem_data from dump dir */
            struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
            if (!dd)
                return -1;

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
