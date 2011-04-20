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
#ifndef ABRT_CONF_H_
#define ABRT_CONF_H_

#ifdef __cplusplus
extern "C" {
#endif

#define g_settings_setOpenGPGPublicKeys abrt_g_settings_setOpenGPGPublicKeys
extern GList *       g_settings_setOpenGPGPublicKeys;
#define g_settings_setBlackListedPkgs abrt_g_settings_setBlackListedPkgs
extern GList *       g_settings_setBlackListedPkgs;
#define g_settings_setBlackListedPaths abrt_g_settings_setBlackListedPaths
extern GList *       g_settings_setBlackListedPaths;
#define g_settings_nMaxCrashReportsSize abrt_g_settings_nMaxCrashReportsSize
extern unsigned int  g_settings_nMaxCrashReportsSize;
#define g_settings_bOpenGPGCheck abrt_g_settings_bOpenGPGCheck
extern bool          g_settings_bOpenGPGCheck;
#define g_settings_bProcessUnpackaged abrt_g_settings_bProcessUnpackaged
extern bool          g_settings_bProcessUnpackaged;
#define g_settings_sWatchCrashdumpArchiveDir abrt_g_settings_sWatchCrashdumpArchiveDir
extern char *        g_settings_sWatchCrashdumpArchiveDir;

#define load_abrt_conf abrt_load_abrt_conf
int load_abrt_conf();
#define free_abrt_conf_data abrt_free_abrt_conf_data
void free_abrt_conf_data();

#ifdef __cplusplus
}
#endif

#endif
