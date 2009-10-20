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
#ifndef ABRT_CLI_DBUS_H
#define ABRT_CLI_DBUS_H

#include "abrt_dbus.h"
#include "CrashTypes.h"

extern DBusConnection* s_dbus_conn;

extern DBusMessage* new_call_msg(const char* method);
extern DBusMessage* send_get_reply_and_unref(DBusMessage* msg);
extern vector_crash_infos_t call_GetCrashInfos();
extern map_crash_report_t call_CreateReport(const char* uuid);
extern void call_Report(const map_crash_report_t& report);
extern void call_DeleteDebugDump(const char* uuid);
extern void handle_dbus_err(bool error_flag, DBusError *err);

#endif
