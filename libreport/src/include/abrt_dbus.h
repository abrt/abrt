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
#ifndef ABRT_DBUS_H
#define ABRT_DBUS_H

#include <dbus/dbus.h>
#include "libreport.h"


#define ABRTD_DBUS_NAME "com.redhat.abrt"
#define ABRTD_DBUS_PATH "/com/redhat/abrt"
#define ABRTD_DBUS_IFACE "com.redhat.abrt"


#ifdef __cplusplus
extern "C" {
#endif

extern DBusConnection* g_dbus_conn;

/*
 * Glib integration machinery
 */

/* Hook up to DBus and to glib main loop.
 * Usage cases:
 *
 * - server:
 *  conn = dbus_bus_get(DBUS_BUS_SYSTEM/SESSION, &err);
 *  attach_dbus_conn_to_glib_main_loop(conn, "/some/path", handler_of_calls_to_some_path);
 *  rc = dbus_bus_request_name(conn, "server.name", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
 *
 * - client which does not receive signals (only makes calls and emits signals):
 *  conn = dbus_bus_get(DBUS_BUS_SYSTEM/SESSION, &err);
 *  // needed only if you need to use async dbus calls (not shown below):
 *  attach_dbus_conn_to_glib_main_loop(conn, NULL, NULL);
 *  // synchronous method call:
 *  msg = dbus_message_new_method_call("some.serv", "/path/on/serv", "optional.iface.on.serv", "method_name");
 *  reply = dbus_connection_send_with_reply_and_block(conn, msg, timeout, &err);
 *  // emitting signal:
 *  msg = dbus_message_new_signal("/path/sig/emitted/from", "iface.sig.emitted.from", "sig_name");
 *  // (note: "iface.sig.emitted.from" is not optional for signals!)
 *  dbus_message_set_destination(msg, "peer"); // optional
 *  dbus_connection_send(conn, msg, &serial); // &serial can be NULL
 *  dbus_connection_unref(conn); // if you don't want to *stay* connected
 *
 * - client which receives and processes signals:
 *  conn = dbus_bus_get(DBUS_BUS_SYSTEM/SESSION, &err);
 *  attach_dbus_conn_to_glib_main_loop(conn, NULL, NULL);
 *  dbus_connection_add_filter(conn, handle_message, NULL, NULL)
 *  dbus_bus_add_match(system_conn, "type='signal',...", &err);
 *  // signal is a dbus message which looks like this:
 *  // sender=XXX dest=YYY(or null) path=/path/sig/emitted/from interface=iface.sig.emitted.from member=sig_name
 *  // and handler_for_signals(conn,msg,opaque) will be called by glib
 *  // main loop to process received signals (and other messages
 *  // if you ask for them in dbus_bus_add_match[es], but this
 *  // would turn you into a server if you handle them too) ;]
 */
void attach_dbus_conn_to_glib_main_loop(DBusConnection* conn,
    /* NULL if you are just a client */
    const char* object_path_to_register,
    /* makes sense only if you use object_path_to_register: */
    DBusHandlerResult (*message_received_func)(DBusConnection *conn, DBusMessage *msg, void* data)
);

/* Log dbus error if err has it set. Then log msg if it's !NULL.
 * In both cases return 1. Otherwise return 0.
 */
int log_dbus_error(const char *msg, DBusError *err);

/* Perform "DeleteDebugDump" call over g_dbus_conn */
int32_t call_DeleteDebugDump(const char *dump_dir_name);

/* Connect to system bus, find abrtd, perform "DeleteDebugDump" call, close g_dbus_conn */
/* now static: int connect_to_abrtd_and_call_DeleteDebugDump(const char *dump_dir_name); */
int delete_dump_dir_possibly_using_abrtd(const char *dump_dir_name);


/*
 * Helpers for building DBus messages
 */
//void store_bool(DBusMessageIter* iter, bool val);
void store_int32(DBusMessageIter* iter, int32_t val);
void store_uint32(DBusMessageIter* iter, uint32_t val);
void store_int64(DBusMessageIter* iter, int64_t val);
void store_uint64(DBusMessageIter* iter, uint64_t val);
void store_string(DBusMessageIter* iter, const char* val);

/*
 * Helpers for parsing DBus messages
 */
enum {
    ABRT_DBUS_ERROR = -1,
    ABRT_DBUS_LAST_FIELD = 0,
    ABRT_DBUS_MORE_FIELDS = 1,
    /* note that dbus_message_iter_next() returns FALSE on last field
     * and TRUE if there are more fields.
     * It maps exactly on the above constants. */
};
/* Checks type, loads data, advances to the next arg.
 * Returns TRUE if next arg exists.
 */
//int load_bool(DBusMessageIter* iter, bool& val);
int load_int32(DBusMessageIter* iter, int32_t *val);
int load_uint32(DBusMessageIter* iter, uint32_t *val);
int load_int64(DBusMessageIter* iter, int64_t *val);
int load_uint64(DBusMessageIter* iter, uint64_t *val);
int load_charp(DBusMessageIter* iter, const char **val);

#ifdef __cplusplus
}
#endif


/*
 * C++ style stuff
 */

#ifdef __cplusplus

#include <map>
#include <vector>

/*
 * Helpers for building DBus messages
 */

static inline std::string ssprintf(const char *format, ...)
{
    va_list p;
    char *string_ptr;

    va_start(p, format);
    string_ptr = xvasprintf(format, p);
    va_end(p);

    std::string res = string_ptr;
    free(string_ptr);
    return res;
}

//static inline void store_val(DBusMessageIter* iter, bool val)               { store_bool(iter, val); }
static inline void store_val(DBusMessageIter* iter, int32_t val)            { store_int32(iter, val); }
static inline void store_val(DBusMessageIter* iter, uint32_t val)           { store_uint32(iter, val); }
static inline void store_val(DBusMessageIter* iter, int64_t val)            { store_int64(iter, val); }
static inline void store_val(DBusMessageIter* iter, uint64_t val)           { store_uint64(iter, val); }
static inline void store_val(DBusMessageIter* iter, const char* val)        { store_string(iter, val); }
static inline void store_val(DBusMessageIter* iter, const std::string& val) { store_string(iter, val.c_str()); }

/* Templates for vector and map */
template <typename T> struct abrt_dbus_type {};
//template <> struct abrt_dbus_type<bool>        { static const char* csig() { return "b"; } };
template <> struct abrt_dbus_type<int32_t>     { static const char* csig() { return "i"; } static std::string sig(); };
template <> struct abrt_dbus_type<uint32_t>    { static const char* csig() { return "u"; } static std::string sig(); };
template <> struct abrt_dbus_type<int64_t>     { static const char* csig() { return "x"; } static std::string sig(); };
template <> struct abrt_dbus_type<uint64_t>    { static const char* csig() { return "t"; } static std::string sig(); };
template <> struct abrt_dbus_type<std::string> { static const char* csig() { return "s"; } static std::string sig(); };
#define ABRT_DBUS_SIG(T) (abrt_dbus_type<T>::csig() ? abrt_dbus_type<T>::csig() : abrt_dbus_type<T>::sig().c_str())
template <typename E>
struct abrt_dbus_type< std::vector<E> > {
    static const char* csig() { return NULL; }
    static std::string sig() { return ssprintf("a%s", ABRT_DBUS_SIG(E)); }
};
template <typename K, typename V>
struct abrt_dbus_type< std::map<K,V> > {
    static const char* csig() { return NULL; }
    static std::string sig() { return ssprintf("a{%s%s}", ABRT_DBUS_SIG(K), ABRT_DBUS_SIG(V)); }
};

template <typename E>
static void store_vector(DBusMessageIter* iter, const std::vector<E>& val)
{
    DBusMessageIter sub_iter;
    if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, ABRT_DBUS_SIG(E), &sub_iter))
        die_out_of_memory();

    typename std::vector<E>::const_iterator vit = val.begin();
    for (; vit != val.end(); ++vit)
    {
        store_val(&sub_iter, *vit);
    }

    if (!dbus_message_iter_close_container(iter, &sub_iter))
        die_out_of_memory();
}
/*
template<>
static void store_vector(DBus::MessageIter &iter, const std::vector<uint8_t>& val)
{
    if we use such vector, MUST add specialized code here (see in dbus-c++ source)
}
*/
template <typename K, typename V>
static void store_map(DBusMessageIter* iter, const std::map<K,V>& val)
{
    DBusMessageIter sub_iter;
    if (!dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
                ssprintf("{%s%s}", ABRT_DBUS_SIG(K), ABRT_DBUS_SIG(V)).c_str(),
                &sub_iter))
        die_out_of_memory();

    typename std::map<K,V>::const_iterator mit = val.begin();
    for (; mit != val.end(); ++mit)
    {
        DBusMessageIter sub_sub_iter;
        if (!dbus_message_iter_open_container(&sub_iter, DBUS_TYPE_DICT_ENTRY, NULL, &sub_sub_iter))
            die_out_of_memory();
        store_val(&sub_sub_iter, mit->first);
        store_val(&sub_sub_iter, mit->second);
        if (!dbus_message_iter_close_container(&sub_iter, &sub_sub_iter))
            die_out_of_memory();
    }

    if (!dbus_message_iter_close_container(iter, &sub_iter))
        die_out_of_memory();
}

template <typename E>
static inline void store_val(DBusMessageIter* iter, const std::vector<E>& val) { store_vector(iter, val); }
template <typename K, typename V>
static inline void store_val(DBusMessageIter* iter, const std::map<K,V>& val)  { store_map(iter, val); }


/*
 * Helpers for parsing DBus messages
 */

//static inline int load_val(DBusMessageIter* iter, bool &val)        { return load_bool(iter, &val); }
static inline int load_val(DBusMessageIter* iter, int32_t &val)     { return load_int32(iter, &val); }
static inline int load_val(DBusMessageIter* iter, uint32_t &val)    { return load_uint32(iter, &val); }
static inline int load_val(DBusMessageIter* iter, int64_t &val)     { return load_int64(iter, &val); }
static inline int load_val(DBusMessageIter* iter, uint64_t &val)    { return load_uint64(iter, &val); }
static inline int load_val(DBusMessageIter* iter, const char*& val) { return load_charp(iter, &val); }
static inline int load_val(DBusMessageIter* iter, std::string& val)
{
    const char* str;
    int r = load_charp(iter, &str);
    val = str;
    return r;
}

/* Templates for vector and map */
template <typename E>
static int load_vector(DBusMessageIter* iter, std::vector<E>& val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_ARRAY)
    {
        error_msg("array expected in dbus message, but not found ('%c')", type);
        return -1;
    }

    DBusMessageIter sub_iter;
    dbus_message_iter_recurse(iter, &sub_iter);

    int r;
//int cnt = 0;
    /* When the vector has 0 elements, we see DBUS_TYPE_INVALID here */
    type = dbus_message_iter_get_arg_type(&sub_iter);
    if (type != DBUS_TYPE_INVALID)
    {
        do {
            E elem;
//cnt++;
            r = load_val(&sub_iter, elem);
            if (r < 0)
                return r;
            val.push_back(elem);
        } while (r == ABRT_DBUS_MORE_FIELDS);
    }
//log("%s: %d elems", __func__, cnt);

    return dbus_message_iter_next(iter);
}
/*
template<>
static int load_vector(DBusMessageIter* iter, std::vector<uint8_t>& val)
{
    if we use such vector, MUST add specialized code here (see in dbus-c++ source)
}
*/
template <typename K, typename V>
static int load_map(DBusMessageIter* iter, std::map<K,V>& val)
{
    int type = dbus_message_iter_get_arg_type(iter);
    if (type != DBUS_TYPE_ARRAY)
    {
        error_msg("array expected in dbus message, but not found ('%c')", type);
        return -1;
    }

    DBusMessageIter sub_iter;
    dbus_message_iter_recurse(iter, &sub_iter);

    bool next_exists;
    int r;
//int cnt = 0;
    do {
        type = dbus_message_iter_get_arg_type(&sub_iter);
        if (type != DBUS_TYPE_DICT_ENTRY)
        {
            /* When the map has 0 elements, we see DBUS_TYPE_INVALID (on the first iteration) */
            if (type == DBUS_TYPE_INVALID)
                break;
            error_msg("sub_iter type is not DBUS_TYPE_DICT_ENTRY (%c)!", type);
            return -1;
        }

        DBusMessageIter sub_sub_iter;
        dbus_message_iter_recurse(&sub_iter, &sub_sub_iter);

        K key;
        r = load_val(&sub_sub_iter, key);
        if (r != ABRT_DBUS_MORE_FIELDS)
        {
            if (r == ABRT_DBUS_LAST_FIELD)
                error_msg("malformed map element in dbus message");
            return -1;
        }
        V value;
        r = load_val(&sub_sub_iter, value);
        if (r != ABRT_DBUS_LAST_FIELD)
        {
            if (r == ABRT_DBUS_MORE_FIELDS)
                error_msg("malformed map element in dbus message");
            return -1;
        }
        val[key] = value;
//cnt++;
        next_exists = dbus_message_iter_next(&sub_iter);
    } while (next_exists);
//log("%s: %d elems", __func__, cnt);

    return dbus_message_iter_next(iter);
}

template <typename E>
static inline int load_val(DBusMessageIter* iter, std::vector<E>& val) { return load_vector(iter, val); }
template <typename K, typename V>
static inline int load_val(DBusMessageIter* iter, std::map<K,V>& val)  { return load_map(iter, val); }

#endif /* __cplusplus */

#endif
