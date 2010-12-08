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
#include "dbus_common.h"
#include "report.h"
#include "dbus.h"

/** Creates a localized string from crash time. */
static char *localize_crash_time(const char *timestr)
{
    long time = xatou(timestr);
    char timeloc[256];
    int success = strftime(timeloc, 128, "%c", localtime(&time));
    if (!success)
        error_msg_and_die("Error while converting time to string");
    return xasprintf("%s", timeloc);
}

/** Prints basic information about a crash to stdout. */
static void print_crash(crash_data_t *crash_data)
{
    /* Create a localized string from crash time. */
    const char *timestr = get_crash_item_content_or_die(crash_data, FILENAME_TIME);
    const char *timeloc = localize_crash_time(timestr);

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

    free((void *)timeloc);

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
    for (unsigned i = 0; i < crash_list->len; ++i)
    {
        crash_data_t *crash = get_crash_data(crash_list, i);
        if (!include_reported)
        {
            const char *msg = get_crash_item_content_or_NULL(crash, FILENAME_MESSAGE);
            if (!msg || !msg[0])
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
    const char *timeloc = localize_crash_time(timestr);

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
           get_crash_item_content_or_die(crash_data, FILENAME_RELEASE),
           get_crash_item_content_or_die(crash_data, FILENAME_KERNEL),
           get_crash_item_content_or_die(crash_data, FILENAME_REASON)
    );

    free((void *)timeloc);

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

    const char *reproduce = get_crash_item_content_or_NULL(crash_data, FILENAME_REPRODUCE);
    if (reproduce)
        printf(_("\nHow to reproduce:\n%s\n"), reproduce);

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

/**
 * Converts crash reference from user's input to crash dump dir name.
 * The returned string must be released by caller.
 */
static char *guess_crash_id(const char *str)
{
    vector_of_crash_data_t *ci = call_GetCrashInfos();
    unsigned num_crashinfos = ci->len;
    if (str[0] == '@') /* "--report @N" syntax */
    {
        unsigned position = xatoi_positive(str + 1);
        if (position >= num_crashinfos)
            error_msg_and_die("There are only %u crash infos", num_crashinfos);
        crash_data_t *info = get_crash_data(ci, position);
        char *res = xstrdup(get_crash_item_content_or_die(info, CD_DUMPDIR));
        free_vector_of_crash_data(ci);
        return res;
    }

    unsigned len = strlen(str);
    unsigned ii;
    char *result = NULL;
    for (ii = 0; ii < num_crashinfos; ii++)
    {
        crash_data_t *info = get_crash_data(ci, ii);
        const char *this_dir = get_crash_item_content_or_die(info, CD_DUMPDIR);
        if (strncmp(str, this_dir, len) == 0)
        {
            if (result)
                error_msg_and_die("Crash prefix '%s' is not unique", str);
            result = xstrdup(this_dir);
        }
    }
    free_vector_of_crash_data(ci);
    if (!result)
        error_msg_and_die("Crash dump directory '%s' not found", str);
    return result;
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
static void usage(char *argv0)
{
    const char *name = progname(argv0);
    printf("%s "VERSION"\n\n", name);

    /* Message has embedded tabs. */
    printf(_("Usage: %s [OPTION]\n\n"
        "Startup:\n"
        "	-V, --version		display the version of %s and exit\n"
        "	-v, --verbose		increase verbosity\n"
        "	-?, --help		print this help\n\n"
        "Actions:\n"
        "	-l, --list		print a list of all crashes which are not yet reported\n"
        "	      -f, --full	print a list of all crashes, including the already reported ones\n"
        "	-r, --report CRASH_ID	create and send a report\n"
        "	      -y, --always	create and send a report without asking\n"
        "	-d, --delete CRASH_ID	remove a crash\n"
        "	-i, --info CRASH_ID	print detailed information about a crash\n"
        "	      -b, --backtrace	print detailed information about a crash including backtrace\n"
        "CRASH_ID can be:\n"
        "	a name of dump directory, or\n"
        "	@N  - N'th crash (as displayed by --list --full) will be acted upon\n"
        ),
        name, name);

    exit(1);
}

int main(int argc, char** argv)
{
    const char* crash_id = NULL;
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

#define SET_OP(newop)                                                \
        if (op != -1 && op != newop)                                 \
        {                                                            \
            error_msg(_("You must specify exactly one operation"));  \
            return 1;                                                \
        }                                                            \
        op = newop;

        switch (c)
        {
        case 'r': SET_OP(OPT_REPORT);   break;
        case 'd': SET_OP(OPT_DELETE);   break;
        case 'l': SET_OP(OPT_GET_LIST); break;
        case 'i': SET_OP(OPT_INFO);     break;
        case 'f': full = true;          break;
        case 'y': always = true;        break;
        case 'b': backtrace = true;     break;
        case 'v': g_verbose++;          break;
        case -1: /* end of options */   break;
        default: /* some error */
        case '?':
            usage(argv[0]); /* exits app */
        case 'V':
            printf("%s "VERSION"\n", progname(argv[0]));
            return 0;
        }
#undef SET_OP
        if (c == -1)
            break;
    }

    /* Handle option arguments. */
    int arg_count = argc - optind;
    switch (arg_count)
    {
    case 0:
        if (op == OPT_REPORT || op == OPT_DELETE || op == OPT_INFO)
            usage(argv[0]);
        break;
    case 1:
        if (op != OPT_REPORT && op != OPT_DELETE && op != OPT_INFO)
            usage(argv[0]);
        crash_id = argv[optind];
        break;
    default:
        usage(argv[0]);
    }

    /* Check if we have an operation.
     * Limit --full and --always to certain operations.
     */
    if ((full && op != OPT_GET_LIST) ||
        (always && op != OPT_REPORT) ||
        (backtrace && op != OPT_INFO) ||
        op == -1)
    {
        usage(argv[0]);
        return 1;
    }

    DBusError err;
    dbus_error_init(&err);
    s_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    handle_dbus_err(s_dbus_conn == NULL, &err);

    /* Do the selected operation. */
    int exitcode = 0;
    switch (op)
    {
        case OPT_GET_LIST:
        {
            vector_of_crash_data_t *ci = call_GetCrashInfos();
            print_crash_list(ci, full);
            free_vector_of_crash_data(ci);
            break;
        }
        case OPT_REPORT:
        {
            int flags = CLI_REPORT_SILENT_IF_NOT_FOUND;
            if (always)
                flags |= CLI_REPORT_BATCH;
            exitcode = report(crash_id, flags);
            if (exitcode == -1) /* no such crash_id */
            {
                crash_id = guess_crash_id(crash_id);
                exitcode = report(crash_id, always ? CLI_REPORT_BATCH : 0);
                if (exitcode == -1)
                {
                    error_msg("Crash '%s' not found", crash_id);
                    free((void *)crash_id);
                    xfunc_die();
                }

                free((void *)crash_id);
            }
            break;
        }
        case OPT_DELETE:
        {
            exitcode = call_DeleteDebugDump(crash_id);
            if (exitcode == ENOENT)
            {
                crash_id = guess_crash_id(crash_id);
                exitcode = call_DeleteDebugDump(crash_id);
                if (exitcode == ENOENT)
                {
                    error_msg("Crash '%s' not found", crash_id);
                    free((void *)crash_id);
                    xfunc_die();
                }

                free((void *)crash_id);
            }
            if (exitcode != 0)
                error_msg_and_die("Can't delete debug dump '%s'", crash_id);
            break;
        }
        case OPT_INFO:
        {
            int old_logmode = logmode;
            logmode = 0;

            crash_data_t *crash_data = call_CreateReport(crash_id);
            if (!crash_data) /* no such crash_id */
            {
                crash_id = guess_crash_id(crash_id);
                crash_data = call_CreateReport(crash_id);
                if (!crash_data)
                {
                    error_msg("Crash '%s' not found", crash_id);
                    free((void *)crash_id);
                    xfunc_die();
                }

                free((void *)crash_id);
            }

            logmode = old_logmode;

            print_crash_info(crash_data, backtrace);
            free_crash_data(crash_data);

            break;
        }
    }

    return exitcode;
}
