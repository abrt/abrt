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
#include "ABRTException.h"
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "DBusCommon.h"
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

/* Program options */
enum
{
    OPT_VERSION,
    OPT_HELP,
    OPT_GET_LIST,
    OPT_GET_LIST_FULL,
    OPT_REPORT,
    OPT_REPORT_ALWAYS,
    OPT_DELETE
};

/** Prints basic information about a crash to stdout. */
static void print_crash(const map_crash_data_t &crash)
{
    /* Create a localized string from crash time. */
    const char *timestr = get_crash_data_item_content(crash, FILENAME_TIME).c_str();
    long time = xatou(timestr);
    char timeloc[256];
    int success = strftime(timeloc, 128, "%c", localtime(&time));
    if (!success)
        error_msg_and_die("Error while converting time to string.");

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

static const struct option longopts[] =
{
    /* name, has_arg, flag, val */
    { "help"         , no_argument      , NULL, OPT_HELP          },
    { "version"      , no_argument      , NULL, OPT_VERSION       },
    { "get-list"     , no_argument      , NULL, OPT_GET_LIST      },
    { "get-list-full", no_argument      , NULL, OPT_GET_LIST_FULL },
    { "report"       , required_argument, NULL, OPT_REPORT        },
    { "report-always", required_argument, NULL, OPT_REPORT_ALWAYS },
    { "delete"       , required_argument, NULL, OPT_DELETE        },
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

/* Prints abrt-cli version and some help text. */
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
        "	--get-list		print list of crashes which are not reported yet\n"
        "	--get-list-full		print list of all crashes\n"
        "	--report CRASH_ID	create and send a report\n"
        "	--report-always CRASH_ID create and send a report without asking\n"
        "	--delete CRASH_ID	remove crash\n"
        "CRASH_ID can be:\n"
        "	UID:UUID pair,\n"
        "	unique UUID prefix  - the crash with matching UUID will be acted upon\n"
        "	@N  - N'th crash (as displayed by --get-list-full) will be acted upon\n"
        ),
        name, name);
}

int main(int argc, char** argv)
{
    const char* crash_id = NULL;
    int op = -1;

    setlocale(LC_ALL, "");
#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    while (1)
    {
        int option_index;
        int c = getopt_long_only(argc, argv, "?V", longopts, &option_index);
        switch (c)
        {
        case OPT_REPORT:
        case OPT_REPORT_ALWAYS:
        case OPT_DELETE:
            crash_id = optarg;
            /* fall through */
        case OPT_GET_LIST:
        case OPT_GET_LIST_FULL:
            if (op == -1)
                break;
            error_msg(_("You must specify exactly one operation."));
            return 1;
        case -1: /* end of options */
            if (op != -1) /* if some operation was specified... */
                break;
            /* fall through */
        default:
        case '?':
        case OPT_HELP:
            usage(argv[0]);
            return 1;
        case 'V':
        case OPT_VERSION:
            printf("%s "VERSION"\n", progname(argv[0]));
            return 0;
        }
        if (c == -1)
            break;
        op = c;
    }

#ifdef ENABLE_DBUS
    DBusError err;
    dbus_error_init(&err);
    s_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    handle_dbus_err(s_dbus_conn == NULL, &err);
#elif ENABLE_SOCKET
    CABRTSocket ABRTDaemon;
    ABRTDaemon.Connect(VAR_RUN"/abrt.socket");
#endif

    int exitcode = 0;
    switch (op)
    {
        case OPT_GET_LIST:
        case OPT_GET_LIST_FULL:
        {
            vector_map_crash_data_t ci = call_GetCrashInfos();
            print_crash_list(ci, op == OPT_GET_LIST_FULL);
            break;
        }
        case OPT_REPORT:
        case OPT_REPORT_ALWAYS:
            exitcode = report(crash_id, (op == OPT_REPORT_ALWAYS)*CLI_REPORT_BATCH + CLI_REPORT_SILENT_IF_NOT_FOUND);
            if (exitcode == -1) /* no such crash_id */
            {
                crash_id = guess_crash_id(crash_id);
                exitcode = report(crash_id, (op == OPT_REPORT_ALWAYS) * CLI_REPORT_BATCH);
                if (exitcode == -1)
                    error_msg_and_die("Crash '%s' not found", crash_id);
            }
            break;
        case OPT_DELETE:
        {
            exitcode = call_DeleteDebugDump(crash_id);
            if (exitcode == ENOENT)
            {
                crash_id = guess_crash_id(crash_id);
                exitcode = call_DeleteDebugDump(crash_id);
                if (exitcode == ENOENT)
                {
                    error_msg_and_die("Crash '%s' not found", crash_id);
                }
            }
            if (exitcode != 0)
            {
                error_msg_and_die("Can't delete debug dump '%s'", crash_id);
            }
            break;
        }
    }

#if ENABLE_SOCKET
    ABRTDaemon.Disconnect();
#endif

    return exitcode;
}
