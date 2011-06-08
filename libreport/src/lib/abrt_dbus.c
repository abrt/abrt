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
//#include "abrtlib.h"
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

/* dbus daemon will simply close our connection if we send broken utf8.
 * Therefore we must never do that.
 */
static char *sanitize_utf8(const char *src)
{
    const char *initial_src = src;
    char *sanitized = NULL;
    unsigned sanitized_pos = 0;

    while (*src)
    {
        int bytes = 0;

        unsigned c = (unsigned char) *src;
        if (c <= 0x7f)
        {
            bytes = 1;
            goto good_byte;
        }

        /* Unicode -> utf8: */
        /* 80-7FF -> 110yyyxx 10xxxxxx */
        /* 800-FFFF -> 1110yyyy 10yyyyxx 10xxxxxx */
        /* 10000-1FFFFF -> 11110zzz 10zzyyyy 10yyyyxx 10xxxxxx */
        /* 200000-3FFFFFF -> 111110tt 10zzzzzz 10zzyyyy 10yyyyxx 10xxxxxx */
        /* 4000000-FFFFFFFF -> 111111tt 10tttttt 10zzzzzz 10zzyyyy 10yyyyxx 10xxxxxx */
        do {
            c <<= 1;
            bytes++;
        } while ((c & 0x80) && bytes < 6);
        if (bytes == 1)
        {
            /* A bare "continuation" byte. Say, 80 */
            goto bad_byte;
        }

        c = (uint8_t)(c) >> bytes;
        {
            const char *pp = src;
            int cnt = bytes;
            while (--cnt)
            {
                unsigned ch = (unsigned char) *++pp;
                if ((ch & 0xc0) != 0x80) /* Missing "continuation" byte. Example: e0 80 */
                {
                    goto bad_byte;
                }
                c = (c << 6) + (ch & 0x3f);
            }
        }
        /* TODO */
        /* Need to check that c isn't produced by overlong encoding */
        /* Example: 11000000 10000000 converts to NUL */
        /* 11110000 10000000 10000100 10000000 converts to 0x100 */
        /* correct encoding: 11000100 10000000 */
        if (c <= 0x7f) /* crude check: only catches bad encodings which map to chars <= 7f */
        {
            goto bad_byte;
        }

 good_byte:
        while (--bytes >= 0)
        {
            c = (unsigned char) *src++;
            if (sanitized)
            {
                sanitized = (char*) xrealloc(sanitized, sanitized_pos + 2);
                sanitized[sanitized_pos++] = c;
                sanitized[sanitized_pos] = '\0';
            }
        }
        continue;

 bad_byte:
        if (!sanitized)
        {
            sanitized_pos = src - initial_src;
            sanitized = xstrndup(initial_src, sanitized_pos);
        }
        sanitized = (char*) xrealloc(sanitized, sanitized_pos + 5);
        sanitized[sanitized_pos++] = '[';
        c = (unsigned char) *src++;
        sanitized[sanitized_pos++] = "0123456789ABCDEF"[c >> 4];
        sanitized[sanitized_pos++] = "0123456789ABCDEF"[c & 0xf];
        sanitized[sanitized_pos++] = ']';
        sanitized[sanitized_pos] = '\0';
    }

    if (sanitized)
        VERB2 log("note: bad utf8, converted '%s' -> '%s'", initial_src, sanitized);

    return sanitized; /* usually NULL: the whole string is ok */
}
void store_string(DBusMessageIter* iter, const char* val)
{
    const char *sanitized = sanitize_utf8(val);
    if (!dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, sanitized ? &sanitized : &val))
        die_out_of_memory();
    free((char*)sanitized);
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
int load_int32(DBusMessageIter* iter, int32_t *val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_INT32)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "int32", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, val);
    return dbus_message_iter_next(iter);
}
int load_uint32(DBusMessageIter* iter, uint32_t *val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_UINT32)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "uint32", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, val);
    return dbus_message_iter_next(iter);
}
int load_int64(DBusMessageIter* iter, int64_t *val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_INT64)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "int64", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, val);
    return dbus_message_iter_next(iter);
}
int load_uint64(DBusMessageIter* iter, uint64_t *val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_UINT64)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "uint64", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, val);
    return dbus_message_iter_next(iter);
}
int load_charp(DBusMessageIter* iter, const char** val)
{
    *val = NULL;

    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_STRING)
    {
        error_msg("%s expected in dbus message, but not found ('%c')", "string", type);
        return -1;
    }
    dbus_message_iter_get_basic(iter, val);
//log("load_charp:'%s'", *val);
    return dbus_message_iter_next(iter);
}


/*
 * Glib integration machinery
 */

/* Callback: "glib says dbus fd is active" */
static gboolean handle_dbus_fd(GIOChannel *gio, GIOCondition condition, gpointer data)
{
    DBusWatch *watch = (DBusWatch*)data;

    VERB3 log("%s(gio, condition:%x [bits:IN/PRI/OUT/ERR/HUP...], data)", __func__, (int)condition);

    /* Notify the D-Bus library when a previously-added watch
     * is ready for reading or writing, or has an exception such as a hangup.
     */
    int glib_flags = (int)condition;
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
    return TRUE; /* "glib, do not remove this event source!" */
}

typedef struct watch_app_info_t
{
    GIOChannel *channel;
    guint event_source_id;
    bool watch_enabled;
} watch_app_info_t;
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
            if (dbus_flags & DBUS_WATCH_READABLE) glib_flags |= G_IO_IN;
            if (dbus_flags & DBUS_WATCH_WRITABLE) glib_flags |= G_IO_OUT;
            VERB3 log(" adding watch to glib main loop. dbus_flags:%x glib_flags:%x", dbus_flags, glib_flags);
            app_info->event_source_id = g_io_add_watch(app_info->channel, (GIOCondition)glib_flags, handle_dbus_fd, watch);
        }
        /* else: it was already enabled */
    }
    else
    {
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
 * Simple logging handler for dbus errors.
 */
int log_dbus_error(const char *msg, DBusError *err)
{
    int ret = 0;
    if (dbus_error_is_set(err))
    {
        error_msg("dbus error: %s", err->message);
        ret = 1;
    }
    if (msg)
    {
        error_msg(msg);
        ret = 1;
    }
    return ret;
}


/*
 * Initialization. Works as follows:
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
    if (g_dbus_conn)
        error_msg_and_die("Internal bug: can't connect to more than one dbus");
    g_dbus_conn = conn;

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


/*
 * Support functions for clients
 */

/* helpers */
static DBusMessage* new_call_msg(const char* method)
{
    DBusMessage* msg = dbus_message_new_method_call(ABRTD_DBUS_NAME, ABRTD_DBUS_PATH, ABRTD_DBUS_IFACE, method);
    if (!msg)
        die_out_of_memory();
    return msg;
}

static DBusMessage* send_get_reply_and_unref(DBusMessage* msg)
{
    dbus_uint32_t serial;
    if (TRUE != dbus_connection_send(g_dbus_conn, msg, &serial))
        error_msg_and_die("Error sending DBus message");
    dbus_message_unref(msg);

    while (true)
    {
        DBusMessage *received = dbus_connection_pop_message(g_dbus_conn);
        if (!received)
        {
            if (FALSE == dbus_connection_read_write(g_dbus_conn, -1))
                error_msg_and_die("dbus connection closed");
            continue;
        }

        int tp = dbus_message_get_type(received);
        const char *error_str = dbus_message_get_error_name(received);
#if 0
        /* Debugging */
        printf("type:%u (CALL:%u, RETURN:%u, ERROR:%u, SIGNAL:%u)\n", tp,
                                DBUS_MESSAGE_TYPE_METHOD_CALL,
                                DBUS_MESSAGE_TYPE_METHOD_RETURN,
                                DBUS_MESSAGE_TYPE_ERROR,
                                DBUS_MESSAGE_TYPE_SIGNAL
        );
        const char *sender = dbus_message_get_sender(received);
        if (sender)
            printf("sender: %s\n", sender);
        const char *path = dbus_message_get_path(received);
        if (path)
            printf("path: %s\n", path);
        const char *member = dbus_message_get_member(received);
        if (member)
            printf("member: %s\n", member);
        const char *interface = dbus_message_get_interface(received);
        if (interface)
            printf("interface: %s\n", interface);
        const char *destination = dbus_message_get_destination(received);
        if (destination)
            printf("destination: %s\n", destination);
        if (error_str)
            printf("error: '%s'\n", error_str);
#endif

        DBusError err;
        dbus_error_init(&err);

        if (dbus_message_is_signal(received, ABRTD_DBUS_IFACE, "Update"))
        {
            const char *update_msg;
            if (!dbus_message_get_args(received, &err,
                                   DBUS_TYPE_STRING, &update_msg,
                                   DBUS_TYPE_INVALID))
            {
                error_msg_and_die("dbus Update message: arguments mismatch");
            }
            printf(">> %s\n", update_msg);
        }
        else if (dbus_message_is_signal(received, ABRTD_DBUS_IFACE, "Warning"))
        {
            const char *warning_msg;
            if (!dbus_message_get_args(received, &err,
                                   DBUS_TYPE_STRING, &warning_msg,
                                   DBUS_TYPE_INVALID))
            {
                error_msg_and_die("dbus Warning message: arguments mismatch");
            }
            log(">! %s", warning_msg);
        }
        else
        if (tp == DBUS_MESSAGE_TYPE_METHOD_RETURN
         && dbus_message_get_reply_serial(received) == serial
        ) {
            return received;
        }
        else
        if (tp == DBUS_MESSAGE_TYPE_ERROR
         && dbus_message_get_reply_serial(received) == serial
        ) {
            error_msg_and_die("dbus call returned error: '%s'", error_str);
        }

        dbus_message_unref(received);
    }
}

int32_t call_DeleteDebugDump(const char *dump_dir_name)
{
    DBusMessage* msg = new_call_msg(__func__ + 5);
    dbus_message_append_args(msg,
            DBUS_TYPE_STRING, &dump_dir_name,
            DBUS_TYPE_INVALID);

    DBusMessage *reply = send_get_reply_and_unref(msg);

    DBusMessageIter in_iter;
    dbus_message_iter_init(reply, &in_iter);

    int32_t result;
    int r = load_int32(&in_iter, &result);
    if (r != ABRT_DBUS_LAST_FIELD) /* more values present, or bad type */
        error_msg_and_die("dbus call %s: return type mismatch", __func__ + 5);

    dbus_message_unref(reply);
    return result;
}

static int connect_to_abrtd_and_call_DeleteDebugDump(const char *dump_dir_name)
{
    DBusError err;
    dbus_error_init(&err);
    g_dbus_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (log_dbus_error(
                g_dbus_conn ? NULL :
                "error requesting system DBus, possible reasons: "
                "dbus config is incorrect; dbus-daemon is not running, "
                "or dbus daemon needs to be restarted to reload dbus config",
                &err
        )
    ) {
        if (g_dbus_conn)
            dbus_connection_unref(g_dbus_conn);
        g_dbus_conn = NULL;
        return 1;
    }

    int ret = call_DeleteDebugDump(dump_dir_name);
    if (ret == ENOENT)
        error_msg("Dump directory '%s' is not found", dump_dir_name);
    else if (ret != 0)
        error_msg("Can't delete dump directory '%s'", dump_dir_name);

    dbus_connection_unref(g_dbus_conn);
    g_dbus_conn = NULL;

    return ret;
}

int delete_dump_dir_possibly_using_abrtd(const char *dump_dir_name)
{
    /* Try to delete it ourselves */
    struct dump_dir *dd = dd_opendir(dump_dir_name, DD_OPEN_READONLY);
    if (dd)
    {
        if (dd->locked) /* it is not readonly */
            return dd_delete(dd);
        dd_close(dd);
    }

    VERB1 log("Deleting '%s' via abrtd dbus call", dump_dir_name);
    return connect_to_abrtd_and_call_DeleteDebugDump(dump_dir_name);
}
