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
#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "abrt_types.h"

typedef map_vector_pair_string_string_t map_analyzer_actions_and_reporters_t;
typedef map_vector_pair_string_string_t map_cron_t;
typedef map_map_string_t map_abrt_settings_t;

extern set_string_t  g_settings_setOpenGPGPublicKeys;
extern set_string_t  g_settings_mapBlackList;
extern unsigned int  g_settings_nMaxCrashReportsSize;
extern bool          g_settings_bOpenGPGCheck;
extern bool          g_settings_bProcessUnpackaged;
extern std::string   g_settings_sDatabase;
extern map_cron_t    g_settings_mapCron;
extern vector_pair_string_string_t g_settings_vectorActionsAndReporters;
extern map_analyzer_actions_and_reporters_t g_settings_mapAnalyzerActionsAndReporters;

void LoadSettings();
void SaveSettings();
void SetSettings(const map_abrt_settings_t& pSettings, const char * dbus_sender);
map_abrt_settings_t GetSettings();

#endif
