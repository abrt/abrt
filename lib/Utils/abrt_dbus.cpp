#include <dbus/dbus.h>
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
