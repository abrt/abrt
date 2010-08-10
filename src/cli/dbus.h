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
#include "crash_types.h"

extern DBusConnection* s_dbus_conn;

vector_map_crash_data_t call_GetCrashInfos();

map_crash_data_t call_CreateReport(const char *crash_id);

/** Sends report using enabled Reporter plugins.
 * @param report
 *  The report sent to Reporter plugins.
 * @param reporters
 *  List of names of Reporters which should be called.
 * @param plugins
 *  Optional settings for Reporter plugins, can be empty.
 *  Format: plugins["PluginName"]["SettingsKey"] = "SettingsValue"
 *  If it contains settings for some plugin, it must contain _all fields_
 *  obtained by call_GetPluginSettings, otherwise the plugin might ignore
 *  the settings.
 */
report_status_t call_Report(const map_crash_data_t& report,
			    const vector_string_t& reporters,
			    const map_map_string_t &plugins);

int32_t call_DeleteDebugDump(const char* crash_id);

/* Gets basic data about all installed plugins.
 * @todo
 *  Return more semantically structured output - maybe a struct instead of a map.
 */
map_map_string_t call_GetPluginsInfo();

/** Gets default plugin settings.
 * @param name
 *    Corresponds to name obtained from call_GetPluginsInfo.
 */
map_plugin_settings_t call_GetPluginSettings(const char *name);

/** Gets global daemon settings.
 * @todo
 *   Return more semantically structured output - maybe a struct instead of a map.
 */
map_map_string_t call_GetSettings();

void handle_dbus_err(bool error_flag, DBusError *err);

#endif
