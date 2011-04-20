/*
    Copyright (C) 2010  ABRT team
    Copyright (C) 2010  RedHat Inc

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
#include <dbus/dbus.h>
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "comm_layer_inner.h"
#include "MiddleWare.h"
#include "CommLayerServerDBus.h"

/*
 * DBus signal emitters
 */

/* helpers */
static DBusMessage* new_signal_msg(const char* member, const char* peer)
{
    /* path, interface, member name */
    DBusMessage* msg = dbus_message_new_signal(ABRTD_DBUS_PATH, ABRTD_DBUS_IFACE, member);
    if (!msg)
        die_out_of_memory();
    /* Send unicast dbus signal if peer is known */
    if (peer && !dbus_message_set_destination(msg, peer))
        die_out_of_memory();
    return msg;
}
static void send_flush_and_unref(DBusMessage* msg)
{
    if (!g_dbus_conn)
    {
        /* Not logging this, it may recurse */
        return;
    }
    if (!dbus_connection_send(g_dbus_conn, msg, NULL /* &serial */))
        error_msg_and_die("Error sending DBus message");
    dbus_connection_flush(g_dbus_conn);
    VERB3 log("DBus message sent");
    dbus_message_unref(msg);
}

/* Notify the clients (UI) about a new crash */
void send_dbus_sig_Crash(const char *package_name,
                                  const char *dir,
                                  const char *uid_str
) {
    DBusMessage* msg = new_signal_msg("Crash", NULL);
    if (uid_str)
    {
        dbus_message_append_args(msg,
                DBUS_TYPE_STRING, &package_name,
                DBUS_TYPE_STRING, &dir,
                DBUS_TYPE_STRING, &uid_str,
                DBUS_TYPE_INVALID);
        VERB2 log("Sending signal Crash('%s','%s','%s')", package_name, dir, uid_str);
    }
    else
    {
        dbus_message_append_args(msg,
                DBUS_TYPE_STRING, &package_name,
                DBUS_TYPE_STRING, &dir,
                DBUS_TYPE_INVALID);
        VERB2 log("Sending signal Crash('%s','%s')", package_name, dir);
    }
    send_flush_and_unref(msg);
}

void send_dbus_sig_QuotaExceeded(const char* str)
{
    DBusMessage* msg = new_signal_msg("QuotaExceeded", NULL);
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &str,
            DBUS_TYPE_INVALID);
    VERB2 log("Sending signal QuotaExceeded('%s')", str);
    send_flush_and_unref(msg);
}

void send_dbus_sig_Update(const char* pMessage, const char* peer)
{
    DBusMessage* msg = new_signal_msg("Update", peer);
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &pMessage,
            DBUS_TYPE_INVALID);
    send_flush_and_unref(msg);
}

void send_dbus_sig_Warning(const char* pMessage, const char* peer)
{
    DBusMessage* msg = new_signal_msg("Warning", peer);
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &pMessage,
            DBUS_TYPE_INVALID);
    send_flush_and_unref(msg);
}


/*
 * DBus call handlers
 */

static long get_remote_uid(DBusMessage* call, const char** ppSender)
{
    DBusError err;
    dbus_error_init(&err);
    const char* sender = dbus_message_get_sender(call);
    if (ppSender)
        *ppSender = sender;
    long uid = dbus_bus_get_unix_user(g_dbus_conn, sender, &err);
    if (dbus_error_is_set(&err))
    {
        dbus_error_free(&err);
        error_msg("Can't determine remote uid, assuming 0");
        return 0;
    }
    return uid;
}

static int handle_DeleteDebugDump(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    const char* crash_id;
    r = load_charp(&in_iter, &crash_id);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    long unix_uid = get_remote_uid(call, NULL);
    int32_t result = DeleteDebugDump(crash_id, unix_uid);

    DBusMessageIter out_iter;
    dbus_message_iter_init_append(reply, &out_iter);
    store_int32(&out_iter, result);

    send_flush_and_unref(reply);
    return 0;
}

/*
 * Glib integration machinery
 */

/* Callback: "a message is received to a registered object path" */
static DBusHandlerResult message_received(DBusConnection* conn, DBusMessage* msg, void* data)
{
    const char* member = dbus_message_get_member(msg);
    VERB1 log("%s(method:'%s')", __func__, member);

    set_client_name(dbus_message_get_sender(msg));

    DBusMessage* reply = dbus_message_new_method_return(msg);
    int r = -1;
    if (strcmp(member, "DeleteDebugDump") == 0)
        r = handle_DeleteDebugDump(msg, reply);

// NB: C++ binding also handles "Introspect" method, which returns a string.
// It was sending "dummy" introspection answer whick looks like this:
// "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
// "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
// "<node>\n"
// "</node>\n"
// Apart from a warning from abrt-gui, just sending error back works as well.
// NB2: we may want to handle "Disconnected" here too.

    if (r < 0)
    {
        /* handle_XXX experienced an error (and did not send any reply) */
        dbus_message_unref(reply);
        if (dbus_message_get_type(msg) == DBUS_MESSAGE_TYPE_METHOD_CALL)
        {
            /* Create and send error reply */
            reply = dbus_message_new_error(msg, DBUS_ERROR_FAILED, "not supported");
            if (!reply)
                die_out_of_memory();
            send_flush_and_unref(reply);
        }
    }

    set_client_name(NULL);

    return DBUS_HANDLER_RESULT_HANDLED;
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
            "Error requesting DBus name %s, possible reasons: "
            "abrt run by non-root; dbus config is incorrect; "
            "or dbus daemon needs to be restarted to reload dbus config",
            ABRTD_DBUS_NAME);
}

int init_dbus()
{
    DBusConnection* conn;
    DBusError err;

    dbus_error_init(&err);
    VERB3 log("dbus_bus_get");
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    handle_dbus_err(conn == NULL, &err);
    // dbus api says:
    // "If dbus_bus_get obtains a new connection object never before returned
    // from dbus_bus_get(), it will call dbus_connection_set_exit_on_disconnect(),
    // so the application will exit if the connection closes. You can undo this
    // by calling dbus_connection_set_exit_on_disconnect() yourself after you get
    // the connection."
    // ...
    // "When a connection is disconnected, you are guaranteed to get a signal
    // "Disconnected" from the interface DBUS_INTERFACE_LOCAL, path DBUS_PATH_LOCAL"
    //
    // dbus-daemon drops connections if it recvs a malformed message
    // (we actually observed this when we sent bad UTF-8 string).
    // Currently, in this case abrtd just exits with exit code 1.
    // (symptom: last two log messages are "abrtd: remove_watch()")
    // If we want to have better logging or other nontrivial handling,
    // here we need to do:
    //
    //dbus_connection_set_exit_on_disconnect(conn, FALSE);
    //dbus_connection_add_filter(conn, handle_message, NULL, NULL);
    //
    // and need to code up handle_message to check for "Disconnected" dbus signal

    /* Also sets g_dbus_conn to conn. */
    attach_dbus_conn_to_glib_main_loop(conn, "/com/redhat/abrt", message_received);

    VERB3 log("dbus_bus_request_name");
    int rc = dbus_bus_request_name(conn, ABRTD_DBUS_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
//maybe check that r == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER instead?
    handle_dbus_err(rc < 0, &err);
    VERB3 log("dbus init done");

    /* dbus_bus_request_name can already read some data. For example,
     * if we were autostarted, the call which caused autostart arrives
     * at this moment. Thus while dbus fd hasn't any data anymore,
     * dbus library can buffer a message or two.
     * If we don't do this, the data won't be processed
     * until next dbus data arrives.
     */
    int cnt = 10;
    while (dbus_connection_dispatch(conn) != DBUS_DISPATCH_COMPLETE && --cnt)
        VERB3 log("processed initial buffered dbus message");

    return 0;
}

void deinit_dbus()
{
    if (g_dbus_conn != NULL)
    {
        dbus_connection_unref(g_dbus_conn);
        g_dbus_conn = NULL;
    }
}
