#include <dbus/dbus.h>
#include <glib.h>
#include "abrtlib.h"
#include "abrt_dbus.h"

DBusConnection* g_dbus_conn;


/*
 * Helpers for building DBus messages
 */

//void store_bool(DBusMessageIter* iter, bool val)
//{
//    dbus_bool_t db = val;
//    if (!dbus_message_iter_append_basic(iter, DBUS_TYPE_BOOLEAN, &db))
//        die_out_of_memory();
//}
void store_int32(DBusMessageIter* iter, int32_t val)
{
    if (!dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &val))
        die_out_of_memory();
}
void store_uint32(DBusMessageIter* iter, uint32_t val)
{
    if (!dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &val))
        die_out_of_memory();
}
void store_int64(DBusMessageIter* iter, int64_t val)
{
    if (!dbus_message_iter_append_basic(iter, DBUS_TYPE_INT64, &val))
        die_out_of_memory();
}
void store_uint64(DBusMessageIter* iter, uint64_t val)
{
    if (!dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT64, &val))
        die_out_of_memory();
}
void store_string(DBusMessageIter* iter, const char* val)
{
    if (!dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &val))
        die_out_of_memory();
}


/*
 * Helpers for parsing DBus messages
 */

//int load_bool(DBusMessageIter* iter, bool& val)
//{
//    int type = dbus_message_iter_get_arg_type(iter);
//    if (type != DBUS_TYPE_BOOLEAN)
//        error_msg_and_die("%s expected in dbus message, but not found ('%c')", "bool", type);
//    dbus_bool_t db;
//    dbus_message_iter_get_basic(iter, &db);
//    val = db;
//    return dbus_message_iter_next(iter);
//}
int load_int32(DBusMessageIter* iter, int32_t& val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_INT32)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "int32", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, &val);
    return dbus_message_iter_next(iter);
}
int load_uint32(DBusMessageIter* iter, uint32_t& val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_UINT32)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "uint32", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, &val);
    return dbus_message_iter_next(iter);
}
int load_int64(DBusMessageIter* iter, int64_t& val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_INT64)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "int64", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, &val);
    return dbus_message_iter_next(iter);
}
int load_uint64(DBusMessageIter* iter, uint64_t& val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_UINT64)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "uint64", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, &val);
    return dbus_message_iter_next(iter);
}
int load_charp(DBusMessageIter* iter, const char*& val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_STRING)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "string", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, &val);
//log("load_charp:'%s'", val);
    return dbus_message_iter_next(iter);
}


/*
 * Glib integration machinery
 */

/* Callback: "glib says dbus fd is active" */
static gboolean handle_dbus_fd(GIOChannel *gio, GIOCondition condition, gpointer data)
{
    DBusWatch *watch = (DBusWatch*)data;

    VERB3 log("%s(gio, condition:%x [bits:IN/PRI/OUT/ERR/HUP...], data)", __func__, int(condition));

    /* Notify the D-Bus library when a previously-added watch
     * is ready for reading or writing, or has an exception such as a hangup.
     */
    int glib_flags = int(condition);
    int dbus_flags = 0;
    if (glib_flags & G_IO_IN)  dbus_flags |= DBUS_WATCH_READABLE;
    if (glib_flags & G_IO_OUT) dbus_flags |= DBUS_WATCH_WRITABLE;
    if (glib_flags & G_IO_ERR) dbus_flags |= DBUS_WATCH_ERROR;
    if (glib_flags & G_IO_HUP) dbus_flags |= DBUS_WATCH_HANGUP;
    /*
     * TODO:
     * If dbus_watch_handle returns FALSE, then the file descriptor
     * may still be ready for reading or writing, but more memory
     * is needed in order to do the reading or writing. If you ignore
     * the FALSE return, your application may spin in a busy loop
     * on the file descriptor until memory becomes available,
     * but nothing more catastrophic should happen.
     */
    dbus_watch_handle(watch, dbus_flags);

    while (dbus_connection_dispatch(g_dbus_conn) == DBUS_DISPATCH_DATA_REMAINS)
        VERB3 log("%s: more data to process, looping", __func__);
    return TRUE; /* "glib, do not remove this even source!" */
}

struct watch_app_info_t
{
    GIOChannel *channel;
    guint event_source_id;
    bool watch_enabled;
};
/* Callback: "dbus_watch_get_enabled() may return a different value than it did before" */
static void toggled_watch(DBusWatch *watch, void* data)
{
    VERB3 log("%s(watch:%p, data)", __func__, watch);

    watch_app_info_t* app_info = (watch_app_info_t*)dbus_watch_get_data(watch);
    if (dbus_watch_get_enabled(watch))
    {
        if (!app_info->watch_enabled)
        {
            app_info->watch_enabled = true;
            int dbus_flags = dbus_watch_get_flags(watch);
            int glib_flags = 0;
            if (dbus_flags & DBUS_WATCH_READABLE)
                glib_flags |= G_IO_IN;
            if (dbus_flags & DBUS_WATCH_WRITABLE)
                glib_flags |= G_IO_OUT;
            VERB3 log(" adding watch to glib main loop. dbus_flags:%x glib_flags:%x", dbus_flags, glib_flags);
            app_info->event_source_id = g_io_add_watch(app_info->channel, GIOCondition(glib_flags), handle_dbus_fd, watch);
        }
        /* else: it was already enabled */
    } else {
        if (app_info->watch_enabled)
        {
            app_info->watch_enabled = false;
            /* does it free the hidden GSource too? */
            VERB3 log(" removing watch from glib main loop");
            g_source_remove(app_info->event_source_id);
        }
        /* else: it was already disabled */
    }
}
/* Callback: "libdbus needs a new watch to be monitored by the main loop" */
static dbus_bool_t add_watch(DBusWatch *watch, void* data)
{
    VERB3 log("%s(watch:%p, data)", __func__, watch);

    watch_app_info_t* app_info = (watch_app_info_t*)xzalloc(sizeof(*app_info));
    dbus_watch_set_data(watch, app_info, free);

    int fd = dbus_watch_get_unix_fd(watch);
    VERB3 log(" dbus_watch_get_unix_fd():%d", fd);
    app_info->channel = g_io_channel_unix_new(fd);
    /* _unconditionally_ adding it to event loop would be an error */
    toggled_watch(watch, data);
    return TRUE;
}
/* Callback: "libdbus no longer needs a watch to be monitored by the main loop" */
static void remove_watch(DBusWatch *watch, void* data)
{
    VERB3 log("%s()", __func__);
    watch_app_info_t* app_info = (watch_app_info_t*)dbus_watch_get_data(watch);
    if (app_info->watch_enabled)
    {
        app_info->watch_enabled = false;
        g_source_remove(app_info->event_source_id);
    }
    g_io_channel_unref(app_info->channel);
}

/* Callback: "libdbus needs a new timeout to be monitored by the main loop" */
static dbus_bool_t add_timeout(DBusTimeout *timeout, void* data)
{
    VERB3 log("%s()", __func__);
    return TRUE;
}
/* Callback: "libdbus no longer needs a timeout to be monitored by the main loop" */
static void remove_timeout(DBusTimeout *timeout, void* data)
{
    VERB3 log("%s()", __func__);
}
/* Callback: "dbus_timeout_get_enabled() may return a different value than it did before" */
static void timeout_toggled(DBusTimeout *timeout, void* data)
{
//seems to be never called, let's make it noisy
    error_msg_and_die("%s(): FIXME: some dbus machinery is missing here", __func__);
}

/* Callback: "DBusObjectPathVTable is unregistered (or its connection is freed)" */
static void unregister_vtable(DBusConnection *conn, void* data)
{
    VERB3 log("%s()", __func__);
}

/*
 * Initialization works as follows:
 *
 * we have a DBusConnection* (say, obtained with dbus_bus_get)
 * we call dbus_connection_set_watch_functions
 *  libdbus calls back add_watch(watch:0x2341090, data), this watch is for writing
 *   we call toggled_watch, but it finds that watch is not to be enabled yet
 *  libdbus calls back add_watch(watch:0x23410e0, data), this watch is for reading
 *   we call toggled_watch, it adds watch's fd to glib main loop with POLLIN
 *  (note: these watches are different objects, but they have the same fd)
 * we call dbus_connection_set_timeout_functions
 * we call dbus_connection_register_object_path
 *
 * Note: if user will later call dbus_bus_request_name(conn, ...):
 *  libdbus calls back add_timeout()
 *  libdbus calls back remove_timeout()
 *  note - no callback to timeout_toggled()!
 * (therefore there is no code yet in timeout_toggled (see above), it's not used)
 */
void attach_dbus_conn_to_glib_main_loop(DBusConnection* conn,
        const char* object_path,
        DBusHandlerResult (*message_received_func)(DBusConnection *conn, DBusMessage *msg, void* data)
) {
//do we need this? why?
//log("dbus_connection_set_dispatch_status_function");
//    dbus_connection_set_dispatch_status_function(conn,
//                dispatch, /* void dispatch(DBusConnection *conn, DBusDispatchStatus new_status, void* data) */
//                NULL, /* data */
//                NULL /* free_data_function */
//    )
    VERB3 log("dbus_connection_set_watch_functions");
    if (!dbus_connection_set_watch_functions(conn,
                add_watch,
                remove_watch,
                toggled_watch,
                NULL, /* data */
                NULL /* free_data_function */
                )
    ) {
        die_out_of_memory();
    }
    VERB3 log("dbus_connection_set_timeout_functions");
    if (!dbus_connection_set_timeout_functions(conn,
                add_timeout,
                remove_timeout,
                timeout_toggled,
                NULL, /* data */
                NULL /* free_data_function */
                )
    ) {
        die_out_of_memory();
    }

    if (object_path && message_received_func)
    {
        /* Table */
        const DBusObjectPathVTable vtable = {
            /* .unregister_function = */ unregister_vtable,
            /* .message_function    = */ message_received_func,
        };
        VERB3 log("dbus_connection_register_object_path");
        if (!dbus_connection_register_object_path(conn,
                    object_path,
                    &vtable,
                    NULL /* data */
                    )
        ) {
            die_out_of_memory();
        }
    }
}
