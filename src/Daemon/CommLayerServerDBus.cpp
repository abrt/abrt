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
#include "ABRTException.h"
#include "CrashWatcher.h"
#include "Settings.h"
#include "Daemon.h"
#include "CommLayerServerDBus.h"

// 16kB message limit
#define LIMIT_MESSAGE 16384

#if HAVE_CONFIG_H
    #include <config.h>
#endif
#if ENABLE_NLS
    #include <libintl.h>
    #define _(S) gettext(S)
#else
    #define _(S) (S)
#endif

/*
 * DBus signal emitters
 */

/* helpers */
static DBusMessage* new_signal_msg(const char* member, const char* peer = NULL)
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
    if (!dbus_connection_send(g_dbus_conn, msg, NULL /* &serial */))
        error_msg_and_die("Error sending DBus message");
    dbus_connection_flush(g_dbus_conn);
    VERB3 log("DBus message sent");
    dbus_message_unref(msg);
}

/* Notify the clients (UI) about a new crash */
void CCommLayerServerDBus::Crash(const char *package_name,
                                  const char* crash_id,
                                  const char *uid_str)
{
    DBusMessage* msg = new_signal_msg("Crash");
    if (uid_str)
    {
        dbus_message_append_args(msg,
                DBUS_TYPE_STRING, &package_name,
                DBUS_TYPE_STRING, &crash_id,
                DBUS_TYPE_STRING, &uid_str,
                DBUS_TYPE_INVALID);
        VERB2 log("Sending signal Crash('%s','%s','%s')", package_name, crash_id, uid_str);
    }
    else
    {
        dbus_message_append_args(msg,
                DBUS_TYPE_STRING, &package_name,
                DBUS_TYPE_STRING, &crash_id,
                DBUS_TYPE_INVALID);
        VERB2 log("Sending signal Crash('%s','%s')", package_name, crash_id);
    }
    send_flush_and_unref(msg);
}

void CCommLayerServerDBus::QuotaExceed(const char* str)
{
    DBusMessage* msg = new_signal_msg("QuotaExceed");
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &str,
            DBUS_TYPE_INVALID);
    VERB2 log("Sending signal QuotaExceed('%s')", str);
    send_flush_and_unref(msg);
}

void CCommLayerServerDBus::JobDone(const char* peer)
{
    DBusMessage* msg = new_signal_msg("JobDone", peer);
    VERB2 log("Sending signal JobDone() to peer %s", peer);
    send_flush_and_unref(msg);
}

void CCommLayerServerDBus::Update(const char* pMessage, const char* peer)
{
    DBusMessage* msg = new_signal_msg("Update", peer);
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &pMessage,
            DBUS_TYPE_INVALID);
    send_flush_and_unref(msg);
}

void CCommLayerServerDBus::Warning(const char* pMessage, const char* peer)
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

static long get_remote_uid(DBusMessage* call, const char** ppSender = NULL)
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

static int handle_GetCrashInfos(DBusMessage* call, DBusMessage* reply)
{
    long unix_uid = get_remote_uid(call);
    vector_map_crash_data_t argout1 = GetCrashInfos(unix_uid);

    DBusMessageIter out_iter;
    dbus_message_iter_init_append(reply, &out_iter);
    store_val(&out_iter, argout1);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_StartJob(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    const char* crash_id;
    r = load_val(&in_iter, crash_id);
    if (r != ABRT_DBUS_MORE_FIELDS)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }
    int32_t force;
    r = load_val(&in_iter, force);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    const char* sender;
    long unix_uid = get_remote_uid(call, &sender);
    if (CreateReportThread(crash_id, unix_uid, force, sender) != 0)
        return -1; /* can't create thread (err msg is already logged) */

    send_flush_and_unref(reply);
    return 0;
}

static int handle_CreateReport(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    const char* crash_id;
    r = load_val(&in_iter, crash_id);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    long unix_uid = get_remote_uid(call);
    map_crash_data_t report;
    CreateReport(crash_id, unix_uid, /*force:*/ 0, report);

    DBusMessageIter out_iter;
    dbus_message_iter_init_append(reply, &out_iter);
    store_val(&out_iter, report);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_Report(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);

    map_crash_data_t argin1;
    r = load_val(&in_iter, argin1);
    if (r != ABRT_DBUS_MORE_FIELDS)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }
    const char* comment = get_crash_data_item_content_or_NULL(argin1, FILENAME_COMMENT) ? : "";
    const char* reproduce = get_crash_data_item_content_or_NULL(argin1, FILENAME_REPRODUCE) ? : "";
    const char* errmsg = NULL;
    if (strlen(comment) > LIMIT_MESSAGE)
    {
        errmsg = _("Comment is too long");
    }
    else if (strlen(reproduce) > LIMIT_MESSAGE)
    {
        errmsg = _("'How to reproduce' is too long");
    }
    if (errmsg)
    {
        dbus_message_unref(reply);
        reply = dbus_message_new_error(call, DBUS_ERROR_FAILED, errmsg);
        if (!reply)
            die_out_of_memory();
        send_flush_and_unref(reply);
        return 0;
    }

    /* Second parameter: reporters to use */
    vector_string_t reporters;
    r = load_val(&in_iter, reporters);
    if (r == ABRT_DBUS_ERROR)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    /* Third parameter is optional */
    map_map_string_t user_conf_data;
    if (r == ABRT_DBUS_MORE_FIELDS)
    {
        r = load_val(&in_iter, user_conf_data);
        if (r != ABRT_DBUS_LAST_FIELD)
        {
            error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
            return -1;
        }
    }

#if 0
    //const char * sender = dbus_message_get_sender(call);
    if (!user_conf_data.empty())
    {
        map_map_string_t::const_iterator it_user_conf_data = user_conf_data.begin();
        for (; it_user_conf_data != user_conf_data.end(); it_user_conf_data++)
        {
            std::string PluginName = it_user_conf_data->first;
            std::cout << "plugin name: " << it_user_conf_data->first;
            map_string_t::const_iterator it_plugin_config;
            for    (it_plugin_config = it_user_conf_data->second.begin();
                    it_plugin_config != it_user_conf_data->second.end();
                    it_plugin_config++)
            {
                std::cout << " key: " << it_plugin_config->first << " value: " << it_plugin_config->second << std::endl;
            }
            // this would overwrite the default settings
            //g_pPluginManager->SetPluginSettings(PluginName, sender, plugin_settings);
        }
    }
#endif

    long unix_uid = get_remote_uid(call);
    report_status_t argout1;
    try
    {
        argout1 = Report(argin1, user_conf_data, unix_uid);
    }
    catch (CABRTException &e)
    {
        dbus_message_unref(reply);
        reply = dbus_message_new_error(call, DBUS_ERROR_FAILED, e.what());
        if (!reply)
            die_out_of_memory();
        send_flush_and_unref(reply);
        return 0;
    }

    DBusMessageIter out_iter;
    dbus_message_iter_init_append(reply, &out_iter);
    store_val(&out_iter, argout1);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_DeleteDebugDump(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    const char* crash_id;
    r = load_val(&in_iter, crash_id);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    long unix_uid = get_remote_uid(call);
    int32_t result = DeleteDebugDump(crash_id, unix_uid);

    DBusMessageIter out_iter;
    dbus_message_iter_init_append(reply, &out_iter);
    store_val(&out_iter, result);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_GetPluginsInfo(DBusMessage* call, DBusMessage* reply)
{
    DBusMessageIter out_iter;
    dbus_message_iter_init_append(reply, &out_iter);
    store_val(&out_iter, g_pPluginManager->GetPluginsInfo());

    send_flush_and_unref(reply);
    return 0;
}

static int handle_GetPluginSettings(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    const char* PluginName;
    r = load_val(&in_iter, PluginName);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    //long unix_uid = get_remote_uid(call);
    //VERB1 log("got %s('%s') call from uid %ld", "GetPluginSettings", PluginName, unix_uid);
    map_plugin_settings_t plugin_settings = g_pPluginManager->GetPluginSettings(PluginName); //, to_string(unix_uid).c_str());

    DBusMessageIter out_iter;
    dbus_message_iter_init_append(reply, &out_iter);
    store_val(&out_iter, plugin_settings);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_SetPluginSettings(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    std::string PluginName;
    r = load_val(&in_iter, PluginName);
    if (r != ABRT_DBUS_MORE_FIELDS)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }
    map_plugin_settings_t plugin_settings;
    r = load_val(&in_iter, plugin_settings);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    long unix_uid = get_remote_uid(call);
    VERB1 log("got %s('%s',...) call from uid %ld", "SetPluginSettings", PluginName.c_str(), unix_uid);
    /* Disabled, as we don't use it, we use only temporary user settings while reporting
       this method should be used to change the default setting and thus should
       be protected by polkit
   */
   //FIXME: protect with polkit
//    g_pPluginManager->SetPluginSettings(PluginName, to_string(unix_uid), plugin_settings);

    send_flush_and_unref(reply);
    return 0;
}

#ifdef PLUGIN_DYNAMIC_LOAD_UNLOAD
static int handle_RegisterPlugin(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    const char* PluginName;
    r = load_val(&in_iter, PluginName);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    const char * sender = dbus_message_get_sender(call);
    g_pPluginManager->RegisterPluginDBUS(PluginName, sender);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_UnRegisterPlugin(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    const char* PluginName;
    r = load_val(&in_iter, PluginName);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    const char * sender = dbus_message_get_sender(call);
    g_pPluginManager->UnRegisterPluginDBUS(PluginName, sender);

    send_flush_and_unref(reply);
    return 0;
}
#endif

static int handle_GetSettings(DBusMessage* call, DBusMessage* reply)
{
    map_abrt_settings_t result = GetSettings();

    DBusMessageIter out_iter;
    dbus_message_iter_init_append(reply, &out_iter);
    store_val(&out_iter, result);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_SetSettings(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    map_abrt_settings_t param1;
    r = load_val(&in_iter, param1);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    const char * sender = dbus_message_get_sender(call);
    SetSettings(param1, sender);

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
    if (strcmp(member, "GetCrashInfos") == 0)
        r = handle_GetCrashInfos(msg, reply);
    else if (strcmp(member, "StartJob") == 0)
        r = handle_StartJob(msg, reply);
    else if (strcmp(member, "Report") == 0)
        r = handle_Report(msg, reply);
    else if (strcmp(member, "DeleteDebugDump") == 0)
        r = handle_DeleteDebugDump(msg, reply);
    else if (strcmp(member, "CreateReport") == 0)
        r = handle_CreateReport(msg, reply);
    else if (strcmp(member, "GetPluginsInfo") == 0)
        r = handle_GetPluginsInfo(msg, reply);
    else if (strcmp(member, "GetPluginSettings") == 0)
        r = handle_GetPluginSettings(msg, reply);
    else if (strcmp(member, "SetPluginSettings") == 0)
        r = handle_SetPluginSettings(msg, reply);
#ifdef PLUGIN_DYNAMIC_LOAD_UNLOAD
    else if (strcmp(member, "RegisterPlugin") == 0)
        r = handle_RegisterPlugin(msg, reply);
    else if (strcmp(member, "UnRegisterPlugin") == 0)
        r = handle_UnRegisterPlugin(msg, reply);
#endif
    else if (strcmp(member, "GetSettings") == 0)
        r = handle_GetSettings(msg, reply);
    else if (strcmp(member, "SetSettings") == 0)
        r = handle_SetSettings(msg, reply);
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

CCommLayerServerDBus::CCommLayerServerDBus()
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
    // Currently, in this case abrtd just exits with exitcode 1.
    // (symptom: last two log messages are "abrtd: remove_watch()")
    // If we want to have better logging or other nontrivial handling,
    // here we need to do:
    //
    //dbus_connection_set_exit_on_disconnect(conn, FALSE);
    //dbus_connection_add_filter(conn, handle_message, NULL, NULL);
    //
    // and need to code up handle_message to check for "Disconnected" dbus signal

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
}

CCommLayerServerDBus::~CCommLayerServerDBus()
{
// do we need to do anything here?
}
