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
#include "ABRTSocket.h"
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "DBusCommon.h"
#if HAVE_CONFIG_H
    #include <config.h>
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

static DBusConnection* s_dbus_conn;

static void print_crash_infos(vector_crash_infos_t& pCrashInfos, int pMode)
{
    unsigned int ii;
    for (ii = 0; ii < pCrashInfos.size(); ii++)
    {
        map_crash_info_t& info = pCrashInfos[ii];
        if (pMode == OPT_GET_LIST_FULL || info.find(CD_REPORTED)->second[CD_CONTENT] != "1")
        {
            printf("%u.\n"
                    "\tUID       : %s\n"
                    "\tUUID      : %s\n"
                    "\tPackage   : %s\n"
                    "\tExecutable: %s\n"
                    "\tCrash time: %s\n"
                    "\tCrash Rate: %s\n",
                    ii,
                    info[CD_UID][CD_CONTENT].c_str(),
                    info[CD_UUID][CD_CONTENT].c_str(),
                    info[CD_PACKAGE][CD_CONTENT].c_str(),
                    info[CD_EXECUTABLE][CD_CONTENT].c_str(),
                    info[CD_TIME][CD_CONTENT].c_str(),
                    info[CD_COUNT][CD_CONTENT].c_str()
            );
        }
    }
}

static void print_crash_report(const map_crash_report_t& pCrashReport)
{
    map_crash_report_t::const_iterator it = pCrashReport.begin();
    for (; it != pCrashReport.end(); it++)
    {
        if (it->second[CD_TYPE] != CD_SYS)
        {
            printf("\n%s\n"
                    "-----\n"
                    "%s\n", it->first.c_str(), it->second[CD_CONTENT].c_str());
        }
    }
}

/*
 * DBus member calls
 */

/* helpers */
static DBusMessage* new_call_msg(const char* method)
{
    DBusMessage* msg = dbus_message_new_method_call(CC_DBUS_NAME, CC_DBUS_PATH, CC_DBUS_IFACE, method);
    if (!msg)
        die_out_of_memory();
    return msg;
}
static DBusMessage* send_get_reply_and_unref(DBusMessage* msg)
{
    DBusError err;
    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(s_dbus_conn, msg, /*timeout*/ -1, &err);
    if (reply == NULL)
    {
//TODO: analyse err
        error_msg_and_die("Error sending DBus message");
    }
    dbus_message_unref(msg);
    return reply;
}

static vector_crash_infos_t call_GetCrashInfos()
{
    DBusMessage* msg = new_call_msg("GetCrashInfos");

    DBusMessage *reply = send_get_reply_and_unref(msg);

    vector_crash_infos_t argout;
    DBusMessageIter in_iter;
    dbus_message_iter_init(reply, &in_iter);
    int r = load_val(&in_iter, argout);
    if (r != ABRT_DBUS_LAST_FIELD) /* more values present, or bad type */
        error_msg_and_die("dbus call %s: return type mismatch", "GetCrashInfos");
    dbus_message_unref(reply);
    return argout;
}

static map_crash_report_t call_CreateReport(const char* uuid)
{
    /* Yes, call name is not "CreateReport" but "GetJobResult".
     * We need to clean up the names one day. */
    DBusMessage* msg = new_call_msg("GetJobResult");
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &uuid,
            DBUS_TYPE_INVALID);

    DBusMessage *reply = send_get_reply_and_unref(msg);

    map_crash_report_t argout;
    DBusMessageIter in_iter;
    dbus_message_iter_init(reply, &in_iter);
    int r = load_val(&in_iter, argout);
    if (r != ABRT_DBUS_LAST_FIELD) /* more values present, or bad type */
        error_msg_and_die("dbus call %s: return type mismatch", "GetJobResult");
    dbus_message_unref(reply);
    return argout;
}

static void call_Report(const map_crash_report_t& report)
{
    DBusMessage* msg = new_call_msg("Report");
    DBusMessageIter out_iter;
    dbus_message_iter_init_append(msg, &out_iter);
    store_val(&out_iter, report);

    DBusMessage *reply = send_get_reply_and_unref(msg);
    //it returns a single value of report_status_t type,
    //but we don't use it (yet?)

    dbus_message_unref(reply);
    return;
}

static void call_DeleteDebugDump(const char* uuid)
{
    DBusMessage* msg = new_call_msg("DeleteDebugDump");
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &uuid,
            DBUS_TYPE_INVALID);

    DBusMessage *reply = send_get_reply_and_unref(msg);
    //it returns a single boolean value,
    //but we don't use it (yet?)

    dbus_message_unref(reply);
    return;
}

static void handle_dbus_err(bool error_flag, DBusError *err)
{
    if (dbus_error_is_set(err))
    {
        error_msg("dbus error: %s", err->message);
        /* dbus_error_free(&err); */
        error_flag = true;
    }
    if (!error_flag)
        return;
    error_msg_and_die(
            "error requesting DBus name %s, possible reasons: "
            "abrt run by non-root; dbus config is incorrect",
            CC_DBUS_NAME);
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

/* Gets program name from command line argument. */
static char *progname(char *argv0)
{
    char* name = strrchr(argv0, '/');
    if (name)
        return ++name;
    else
        return argv0;
}

int main(int argc, char** argv)
{
    char* uuid = NULL;
    int op = -1;
    char *name;

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
		error_msg("You must specify exactly one operation.");
                return 1;
	    case -1: /* end of options */
	        if (op != -1) /* if some operation was specified... */
		    break;
                /* fall through */
            default:
  	    case '?':
            case OPT_HELP:
	        name = progname(argv[0]);
  	        printf("%s " VERSION "\n\n", name);
                /* note: message has embedded tabs */
                printf("Usage: %s [OPTION]\n\n"
		        "Startup:\n"
		        "	-V, --version		display the version of %s and exit\n"
		        "	-?, --help		print this help\n\n"
		        "Actions:\n"
                        "	--get-list		print list of crashes which are not reported yet\n"
                        "	--get-list-full		print list of all crashes\n"
                        "	--report UUID		create and send a report\n"
                        "	--report-always UUID	create and send a report without asking\n"
                        "	--delete UUID		remove crash\n",
		       name, name);
                return 1;
            case 'V':
	    case OPT_VERSION:
  	        printf("%s " VERSION "\n", progname(argv[0]));
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
        {
            map_crash_report_t cr = call_CreateReport(uuid);
            print_crash_report(cr);
            printf("\nDo you want to send the report? [y/n]: ");
            fflush(NULL);
            char answer[16] = "n";
            fgets(answer, sizeof(answer), stdin);
            if (answer[0] == 'Y' || answer[0] == 'y')
            {
                call_Report(cr);
            }
            break;
        }
        case OPT_REPORT_ALWAYS:
        {
            map_crash_report_t cr = call_CreateReport(uuid);
            call_Report(cr);
            break;
        }
        case OPT_DELETE:
        {
            call_DeleteDebugDump(uuid);
            break;
        }
    }
#if ENABLE_SOCKET
    ABRTDaemon.DisConnect();
#endif

    return 0;
}
