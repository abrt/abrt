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

/** Creates a localized string from crash time. */
static char *localize_crash_time(const char *timestr)
{
    long time = xatou(timestr);
    char timeloc[256];
    int success = strftime(timeloc, sizeof(timeloc), "%c", localtime(&time));
    if (!success)
        error_msg_and_die("Error while converting time '%s' to string", timestr);
    return xstrdup(timeloc);
}

static crash_data_t *FillCrashInfo(const char *dump_dir_name)
{
    int sv_logmode = logmode;
    logmode = 0; /* suppress EPERM/EACCES errors in opendir */
    struct dump_dir *dd = dd_opendir(dump_dir_name, /*flags:*/ DD_OPEN_READONLY);
    logmode = sv_logmode;

    if (!dd)
        return NULL;

    crash_data_t *crash_data = create_crash_data_from_dump_dir(dd);
    dd_close(dd);
    add_to_crash_data_ext(crash_data, CD_DUMPDIR, dump_dir_name, CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

    return crash_data;
}

static void GetCrashInfos(vector_of_crash_data_t *retval, const char *dir_name)
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
                crash_data_t *crash_data = FillCrashInfo(dump_dir_name);
                if (crash_data)
                    g_ptr_array_add(retval, crash_data);
            }
            free(dump_dir_name);
        }
        closedir(dir);
    }
}

/** Prints basic information about a crash to stdout. */
static void print_crash(crash_data_t *crash_data)
{
    /* Create a localized string from crash time. */
    const char *timestr = get_crash_item_content_or_die(crash_data, FILENAME_TIME);
    char *timeloc = localize_crash_time(timestr);

    printf(_("\tCrash dump : %s\n"
             "\tUID        : %s\n"
             "\tPackage    : %s\n"
             "\tExecutable : %s\n"
             "\tCrash Time : %s\n"
             "\tCrash Count: %s\n"),
           get_crash_item_content_or_NULL(crash_data, CD_DUMPDIR),
           get_crash_item_content_or_NULL(crash_data, FILENAME_UID),
           get_crash_item_content_or_NULL(crash_data, FILENAME_PACKAGE),
           get_crash_item_content_or_NULL(crash_data, FILENAME_EXECUTABLE),
           timeloc,
           get_crash_item_content_or_NULL(crash_data, FILENAME_COUNT)
    );

    free(timeloc);

    /* Print the hostname if it's available. */
    const char *hostname = get_crash_item_content_or_NULL(crash_data, FILENAME_HOSTNAME);
    if (hostname)
        printf(_("\tHostname   : %s\n"), hostname);
}

/**
 * Prints a list containing "crashes" to stdout.
 * @param include_reported
 *   Do not skip entries marked as already reported.
 */
static void print_crash_list(vector_of_crash_data_t *crash_list, bool include_reported)
{
    unsigned i;
    for (i = 0; i < crash_list->len; ++i)
    {
        crash_data_t *crash = get_crash_data(crash_list, i);
        if (!include_reported)
        {
            const char *msg = get_crash_item_content_or_NULL(crash, FILENAME_REPORTED_TO);
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
static void print_crash_info(crash_data_t *crash_data, bool show_backtrace)
{
    const char *timestr = get_crash_item_content_or_die(crash_data, FILENAME_TIME);
    char *timeloc = localize_crash_time(timestr);

    printf(_("Dump directory:     %s\n"
             "Last crash:         %s\n"
             "Analyzer:           %s\n"
             "Component:          %s\n"
             "Package:            %s\n"
             "Command:            %s\n"
             "Executable:         %s\n"
             "System:             %s, kernel %s\n"
             "Reason:             %s\n"),
           get_crash_item_content_or_die(crash_data, CD_DUMPDIR),
           timeloc,
           get_crash_item_content_or_die(crash_data, FILENAME_ANALYZER),
           get_crash_item_content_or_die(crash_data, FILENAME_COMPONENT),
           get_crash_item_content_or_die(crash_data, FILENAME_PACKAGE),
           get_crash_item_content_or_die(crash_data, FILENAME_CMDLINE),
           get_crash_item_content_or_die(crash_data, FILENAME_EXECUTABLE),
           get_crash_item_content_or_die(crash_data, FILENAME_OS_RELEASE),
           get_crash_item_content_or_die(crash_data, FILENAME_KERNEL),
           get_crash_item_content_or_die(crash_data, FILENAME_REASON)
    );

    free(timeloc);

    /* Print optional fields only if they are available */

    /* Coredump is not present in kerneloopses and Python exceptions. */
    const char *coredump = get_crash_item_content_or_NULL(crash_data, FILENAME_COREDUMP);
    if (coredump)
        printf(_("Coredump file:      %s\n"), coredump);

    const char *rating = get_crash_item_content_or_NULL(crash_data, FILENAME_RATING);
    if (rating)
        printf(_("Rating:             %s\n"), rating);

    /* Crash function is not present in kerneloopses, and before the full report is created.*/
    const char *crash_function = get_crash_item_content_or_NULL(crash_data, FILENAME_CRASH_FUNCTION);
    if (crash_function)
        printf(_("Crash function:     %s\n"), crash_function);

    const char *hostname = get_crash_item_content_or_NULL(crash_data, FILENAME_HOSTNAME);
    if (hostname)
        printf(_("Hostname:           %s\n"), hostname);

    const char *comment = get_crash_item_content_or_NULL(crash_data, FILENAME_COMMENT);
    if (comment)
        printf(_("\nComment:\n%s\n"), comment);

    if (show_backtrace)
    {
        const char *backtrace = get_crash_item_content_or_NULL(crash_data, FILENAME_BACKTRACE);
        if (backtrace)
            printf(_("\nBacktrace:\n%s\n"), backtrace);
    }
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
    { "backtrace", no_argument, NULL, 'b' },
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
        "	-l, --list		List not yet reported crashes\n"
        "	  -f, --full		List all crashes\n"
        "	-D BASE_DIR		Directory to list crashes from\n"
        "				(default: -D $HOME/.abrt/spool -D %s)\n"
        "\n"
        "	-r, --report		Send a report about CRASH_DIR\n"
        "	  -y, --always		...without editing and asking\n"
        "	-i, --info		Print detailed information about CRASH_DIR\n"
        "	  -b, --backtrace	...including backtrace\n"
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
    bool backtrace = false;

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
        case 'b': backtrace = true;     break;
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
    if ((full && op != OPT_GET_LIST) ||
        (always && op != OPT_REPORT) ||
        (backtrace && op != OPT_INFO) ||
        op == -1)
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
            vector_of_crash_data_t *ci = new_vector_of_crash_data();
            while (D_list)
            {
                char *dir = (char *)D_list->data;
                GetCrashInfos(ci, dir);
                D_list = g_list_remove(D_list, dir);
            }
            print_crash_list(ci, full);
            free_vector_of_crash_data(ci);
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
            /* Load crash_data from dump dir */
            struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
            if (!dd)
                return -1;

            char *analyze_events_as_lines = list_possible_events(dd, NULL, "analyze");

            if (backtrace && analyze_events_as_lines && *analyze_events_as_lines)
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

                /* Reload crash_data from (possibly updated by analyze) dump dir */
                dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
                if (!dd)
                    return -1;
            } else
                free(analyze_events_as_lines);

            crash_data_t *crash_data = create_crash_data_from_dump_dir(dd);
            dd_close(dd);

            add_to_crash_data_ext(crash_data, CD_DUMPDIR, dump_dir_name,
                                  CD_FLAG_TXT + CD_FLAG_ISNOTEDITABLE);

            print_crash_info(crash_data, backtrace);
            free_crash_data(crash_data);

            break;
        }
    }

    return exitcode;
}
