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

#ifdef __cplusplus
extern "C" {
#endif

extern GList *       g_settings_setOpenGPGPublicKeys;
extern GList *       g_settings_setBlackListedPkgs;
extern GList *       g_settings_setBlackListedPaths;
extern unsigned int  g_settings_nMaxCrashReportsSize;
extern bool          g_settings_bOpenGPGCheck;
extern bool          g_settings_bProcessUnpackaged;
extern char *        g_settings_sWatchCrashdumpArchiveDir;

int load_settings();
void free_settings();

#ifdef __cplusplus
}
#endif

#endif
