/*
    Copyright (C) 2009  RedHat inc.

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

static void print_crash_infos(vector_crash_infos_t& pCrashInfos, int pMode)
{
    unsigned int ii;
    for (ii = 0; ii < pCrashInfos.size(); ii++)
    {
        map_crash_info_t& info = pCrashInfos[ii];
        if (pMode == OPT_GET_LIST_FULL || info[CD_REPORTED][CD_CONTENT] != "1")
        {
            const char *timestr = info[CD_TIME][CD_CONTENT].c_str();
            long time = strtol(timestr, NULL, 10);
            if (time == 0)
                error_msg_and_die("Error while converting time string.");

            char timeloc[256];
            int success = strftime(timeloc, 128, "%c", localtime(&time));
            if (!success)
                error_msg_and_die("Error while converting time to string.");

            printf(_("%u.\n"
                   "\tUID        : %s\n"
                   "\tUUID       : %s\n"
                   "\tPackage    : %s\n"
                   "\tExecutable : %s\n"
                   "\tCrash Time : %s\n"
                   "\tCrash Count: %s\n"),
                 ii,
                 info[CD_UID][CD_CONTENT].c_str(),
                 info[CD_UUID][CD_CONTENT].c_str(),
                 info[CD_PACKAGE][CD_CONTENT].c_str(),
                 info[CD_EXECUTABLE][CD_CONTENT].c_str(),
                 timeloc,
                 info[CD_COUNT][CD_CONTENT].c_str()
            );
        }
    }
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
        "	--report UUID		create and send a report\n"
        "	--report-always UUID	create and send a report without asking\n"
        "	--delete UUID		remove crash\n"),
        name, name);
}

int main(int argc, char** argv)
{
    char* uuid = NULL;
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
                uuid = optarg;
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
            vector_crash_infos_t ci = call_GetCrashInfos();
            print_crash_infos(ci, op);
            break;
        }
        case OPT_REPORT:
            exitcode = report(uuid, false);
            break;
        case OPT_REPORT_ALWAYS:
            exitcode = report(uuid, true);
            break;
        case OPT_DELETE:
        {
            if (call_DeleteDebugDump(uuid) != 0)
            {
                log("Can't delete debug dump with UUID '%s'", uuid);
                exitcode = 1;
            }
            break;
        }
    }

#if ENABLE_SOCKET
    ABRTDaemon.Disconnect();
#endif

    return exitcode;
}
