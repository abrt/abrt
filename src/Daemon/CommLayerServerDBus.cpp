#include <dbus/dbus.h>
#include "abrtlib.h"
#include "abrt_dbus.h"
#include "ABRTException.h"
#include "CrashWatcher.h"
#include "Settings.h"
#include "Daemon.h"
#include "CommLayerServerDBus.h"


#define LIMIT_MESSAGE 1

/*
 * DBus signal emitters
 */

/* helpers */
static DBusMessage* new_signal_msg(const char* member, const char* peer = NULL)
{
    /* path, interface, member name */
    DBusMessage* msg = dbus_message_new_signal(CC_DBUS_PATH, CC_DBUS_IFACE, member);
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
void CCommLayerServerDBus::Crash(const std::string& progname, const std::string& uid)
{
    DBusMessage* msg = new_signal_msg("Crash");
    const char* c_progname = progname.c_str();
    const char* c_uid = uid.c_str();
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &c_progname,
            DBUS_TYPE_STRING, &c_uid,
            DBUS_TYPE_INVALID);
    VERB2 log("Sending signal Crash('%s','%s')", c_progname, c_uid);
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

void CCommLayerServerDBus::JobStarted(const char* peer)
{
    DBusMessage* msg = new_signal_msg("JobStarted", peer);
    uint64_t nJobID = uint64_t(pthread_self());
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &peer, /* TODO: redundant parameter, remove from API */
            DBUS_TYPE_UINT64, &nJobID, /* TODO: redundant parameter, remove from API */
            DBUS_TYPE_INVALID);
    VERB2 log("Sending signal JobStarted('%s',%llx)", peer, (unsigned long long)nJobID);
    send_flush_and_unref(msg);
}

void CCommLayerServerDBus::JobDone(const char* peer, const char* pUUID)
{
    DBusMessage* msg = new_signal_msg("JobDone", peer);
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &peer, /* TODO: redundant parameter, remove from API */
            DBUS_TYPE_STRING, &pUUID, /* TODO: redundant parameter, remove from API */
            DBUS_TYPE_INVALID);
    VERB2 log("Sending signal JobDone('%s','%s')", peer, pUUID);
    send_flush_and_unref(msg);
}

void CCommLayerServerDBus::Update(const std::string& pMessage, const char* peer, uint64_t job_id)
{
    DBusMessage* msg = new_signal_msg("Update", peer);
    const char* c_message = pMessage.c_str();
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &c_message,
            DBUS_TYPE_UINT64, &job_id, /* TODO: redundant parameter, remove from API */
            DBUS_TYPE_INVALID);
    send_flush_and_unref(msg);
}

void CCommLayerServerDBus::Warning(const std::string& pMessage, const char* peer, uint64_t job_id)
{
    DBusMessage* msg = new_signal_msg("Warning", peer);
    const char* c_message = pMessage.c_str();
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &c_message,
            DBUS_TYPE_UINT64, &job_id, /* TODO: redundant parameter, remove from API */
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
    vector_crash_infos_t argout1 = GetCrashInfos(to_string(unix_uid));

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    store_val(&iter, argout1);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_CreateReport(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    const char* pUUID;
    r = load_val(&in_iter, pUUID);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    const char* sender;
    long unix_uid = get_remote_uid(call, &sender);
    if (CreateReportThread(pUUID, to_string(unix_uid).c_str(), sender) != 0)
        return -1; /* can't create thread (err msg is already logged) */

    dbus_message_append_args(reply,
                DBUS_TYPE_STRING, &pUUID, /* redundant, eliminate from API */
                DBUS_TYPE_INVALID);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_GetJobResult(DBusMessage* call, DBusMessage* reply)
{
    int r;
    DBusMessageIter in_iter;
    dbus_message_iter_init(call, &in_iter);
    const char* pUUID;
    r = load_val(&in_iter, pUUID);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    long unix_uid = get_remote_uid(call);
    map_crash_report_t report = GetJobResult(pUUID, to_string(unix_uid).c_str());

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
    map_crash_report_t argin1;
    const char* comment;
    const char* reproduce;

    r = load_val(&in_iter, argin1);
    if (r == ABRT_DBUS_ERROR)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    map_crash_report_t::const_iterator it_comment = argin1.find(CD_COMMENT);
    map_crash_report_t::const_iterator it_reproduce = argin1.find(CD_REPRODUCE);
    comment = (it_comment != argin1.end()) ? it_comment->second[CD_CONTENT].c_str() : "";
    reproduce = (it_reproduce != argin1.end()) ? it_reproduce->second[CD_CONTENT].c_str() : "";

    if( strlen(comment) > LIMIT_MESSAGE )
    {
        dbus_message_unref(reply);
        reply = dbus_message_new_error(call, DBUS_ERROR_FAILED,"Comment message is too long" );
        if (!reply)
            die_out_of_memory();
        send_flush_and_unref(reply);
        return 0;
    }

    if( strlen(reproduce) > LIMIT_MESSAGE )
    {
        dbus_message_unref(reply);
        reply = dbus_message_new_error(call, DBUS_ERROR_FAILED,"How to reproduce message is too long" );
        if (!reply)
            die_out_of_memory();
        send_flush_and_unref(reply);
        return 0;
    }

    /* Second parameter is optional */
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

//so far, user_conf_data is unused
    long unix_uid = get_remote_uid(call);
    report_status_t argout1;
    try
    {
        argout1 = Report(argin1, to_string(unix_uid));
    }
    catch (CABRTException &e)
    {
        dbus_message_unref(reply);
        reply = dbus_message_new_error(call, DBUS_ERROR_FAILED, e.what().c_str());
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
    const char* argin1;
    r = load_val(&in_iter, argin1);
    if (r != ABRT_DBUS_LAST_FIELD)
    {
        error_msg("dbus call %s: parameter type mismatch", __func__ + 7);
        return -1;
    }

    long unix_uid = get_remote_uid(call);
    bool argout1 = DeleteDebugDump(argin1, to_string(unix_uid));

    dbus_message_append_args(reply,
                DBUS_TYPE_BOOLEAN, &argout1,
                DBUS_TYPE_INVALID);

    send_flush_and_unref(reply);
    return 0;
}

static int handle_GetPluginsInfo(DBusMessage* call, DBusMessage* reply)
{
    vector_map_string_t plugins_info = g_pPluginManager->GetPluginsInfo();

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    store_val(&iter, plugins_info);

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

    long unix_uid = get_remote_uid(call);
    VERB1 log("got %s('%s') call from uid %ld", "GetPluginSettings", PluginName, unix_uid);
    map_plugin_settings_t plugin_settings = g_pPluginManager->GetPluginSettings(PluginName, to_string(unix_uid));

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    store_val(&iter, plugin_settings);
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
    g_pPluginManager->SetPluginSettings(PluginName, to_string(unix_uid), plugin_settings);

    send_flush_and_unref(reply);
    return 0;
}

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

static int handle_GetSettings(DBusMessage* call, DBusMessage* reply)
{
    map_abrt_settings_t result = GetSettings();

    DBusMessageIter iter;
    dbus_message_iter_init_append(reply, &iter);
    store_val(&iter, result);
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
    else if (strcmp(member, "CreateReport") == 0)
        r = handle_CreateReport(msg, reply);
    else if (strcmp(member, "Report") == 0)
        r = handle_Report(msg, reply);
    else if (strcmp(member, "DeleteDebugDump") == 0)
        r = handle_DeleteDebugDump(msg, reply);
    else if (strcmp(member, "GetJobResult") == 0)
        r = handle_GetJobResult(msg, reply);
    else if (strcmp(member, "GetPluginsInfo") == 0)
        r = handle_GetPluginsInfo(msg, reply);
    else if (strcmp(member, "GetPluginSettings") == 0)
        r = handle_GetPluginSettings(msg, reply);
    else if (strcmp(member, "SetPluginSettings") == 0)
        r = handle_SetPluginSettings(msg, reply);
    else if (strcmp(member, "RegisterPlugin") == 0)
        r = handle_RegisterPlugin(msg, reply);
    else if (strcmp(member, "UnRegisterPlugin") == 0)
        r = handle_UnRegisterPlugin(msg, reply);
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
            "abrt run by non-root; dbus config is incorrect",
            CC_DBUS_NAME);
}

CCommLayerServerDBus::CCommLayerServerDBus()
{
    DBusConnection* conn;
    DBusError err;

    dbus_error_init(&err);
    VERB3 log("dbus_bus_get");
    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    handle_dbus_err(conn == NULL, &err);

    attach_dbus_conn_to_glib_main_loop(conn, "/com/redhat/abrt", message_received);

    VERB3 log("dbus_bus_request_name");
    int rc = dbus_bus_request_name(conn, CC_DBUS_NAME, DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
//maybe check that r == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER instead?
    handle_dbus_err(rc < 0, &err);
    VERB3 log("dbus init done");
}

CCommLayerServerDBus::~CCommLayerServerDBus()
{
// do we need to do anything here?
}
