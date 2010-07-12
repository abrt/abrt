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
#include <getopt.h>
#include "abrt_exception.h"
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "dbus_common.h"
#include "report.h"
#include "dbus.h"
#if HAVE_CONFIG_H
# include <config.h>
#endif
#if HAVE_LOCALE_H
# include <locale.h>
#endif
#if ENABLE_NLS
# include <libintl.h>
# define _(S) gettext(S)
#else
# define _(S) (S)
#endif

/** Prints basic information about a crash to stdout. */
static void print_crash(const map_crash_data_t &crash)
{
    /* Create a localized string from crash time. */
    const char *timestr = get_crash_data_item_content(crash, FILENAME_TIME).c_str();
    long time = xatou(timestr);
    char timeloc[256];
    int success = strftime(timeloc, 128, "%c", localtime(&time));
    if (!success)
        error_msg_and_die("Error while converting time to string");

    printf(_("\tUID        : %s\n"
             "\tUUID       : %s\n"
             "\tPackage    : %s\n"
             "\tExecutable : %s\n"
             "\tCrash Time : %s\n"
             "\tCrash Count: %s\n"),
           get_crash_data_item_content(crash, CD_UID).c_str(),
           get_crash_data_item_content(crash, CD_UUID).c_str(),
           get_crash_data_item_content(crash, FILENAME_PACKAGE).c_str(),
           get_crash_data_item_content(crash, FILENAME_EXECUTABLE).c_str(),
           timeloc,
           get_crash_data_item_content(crash, CD_COUNT).c_str());

    /* Print the hostname if it's available. */
    const char *hostname = get_crash_data_item_content_or_NULL(crash, FILENAME_HOSTNAME);
    if (hostname)
        printf(_("\tHostname   : %s\n"), hostname);
}

/**
 * Prints a list containing "crashes" to stdout.
 * @param include_reported
 *   Do not skip entries marked as already reported.
 */
static void print_crash_list(const vector_map_crash_data_t& crash_list, bool include_reported)
{
    for (unsigned i = 0; i < crash_list.size(); ++i)
    {
        const map_crash_data_t& crash = crash_list[i];
        if (get_crash_data_item_content(crash, CD_REPORTED) == "1" && !include_reported)
            continue;

        printf("%u.\n", i);
        print_crash(crash);
    }
}

/**
 * Converts crash reference from user's input to unique crash identification
 * in form UID:UUID.
 * The returned string must be released by caller.
 */
static char *guess_crash_id(const char *str)
{
    vector_map_crash_data_t ci = call_GetCrashInfos();
    unsigned num_crashinfos = ci.size();
    if (str[0] == '@') /* "--report @N" syntax */
    {
        unsigned position = xatoi_u(str + 1);
        if (position >= num_crashinfos)
            error_msg_and_die("There are only %u crash infos", num_crashinfos);
        map_crash_data_t& info = ci[position];
        return xasprintf("%s:%s",
                get_crash_data_item_content(info, CD_UID).c_str(),
                get_crash_data_item_content(info, CD_UUID).c_str()
        );
    }

    unsigned len = strlen(str);
    unsigned ii;
    char *result = NULL;
    for (ii = 0; ii < num_crashinfos; ii++)
    {
        map_crash_data_t& info = ci[ii];
        const char *this_uuid = get_crash_data_item_content(info, CD_UUID).c_str();
        if (strncmp(str, this_uuid, len) == 0)
        {
            if (result)
                error_msg_and_die("Crash prefix '%s' is not unique", str);
            result = xasprintf("%s:%s",
                    get_crash_data_item_content(info, CD_UID).c_str(),
                    this_uuid
            );
        }
    }
    if (!result)
        error_msg_and_die("Crash '%s' not found", str);
    return result;
}

/* Program options */
enum
{
    OPT_GET_LIST,
    OPT_REPORT,
    OPT_DELETE
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
    { "help"   , no_argument, NULL, '?' },
    { "version", no_argument, NULL, 'V' },
    { "list"   , no_argument, NULL, 'l' },
    { "full"   , no_argument, NULL, 'f' },
    { "always" , no_argument, NULL, 'y' },
    { "report" , no_argument, NULL, 'r' },
    { "delete" , no_argument, NULL, 'd' },
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
        "	-?, --help		print this help\n\n"
        "Actions:\n"
        "	-l, --list		print a list of all crashes which are not yet reported\n"
        "	      -f, --full	print a list of all crashes, including the already reported ones\n"
        "	-r, --report CRASH_ID	create and send a report\n"
        "	      -y, --always	create and send a report without asking\n"
        "	-d, --delete CRASH_ID	remove a crash\n"
        "CRASH_ID can be:\n"
        "	UID:UUID pair,\n"
        "	unique UUID prefix  - the crash with matching UUID will be acted upon\n"
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

    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    while (1)
    {
        int option_index;
        /* Do not use colons, arguments are handled after parsing all options. */
        int c = getopt_long_only(argc, argv, "?Vrdlfy",
                                 longopts, &option_index);

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
        case 'f': full = true;          break;
        case 'y': always = true;        break;
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
        if (op == OPT_REPORT || op == OPT_DELETE)
            usage(argv[0]);
        break;
    case 1:
        if (op != OPT_REPORT && op != OPT_DELETE)
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
            vector_map_crash_data_t ci = call_GetCrashInfos();
            print_crash_list(ci, full);
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
                    error_msg_and_die("Crash '%s' not found", crash_id);
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
                    error_msg_and_die("Crash '%s' not found", crash_id);
            }
            if (exitcode != 0)
                error_msg_and_die("Can't delete debug dump '%s'", crash_id);
            break;
        }
    }

    return exitcode;
}
