/*
    Copyright (C) 2009  Denys Vlasenko (dvlasenk@redhat.com)
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

#ifndef DAEMON_H_
#define DAEMON_H_

#include <pthread.h>
#include "abrt_types.h"
#include "CrashTypes.h"

class CCrashWatcher;
class CCommLayerServer;
class CPluginManager;

/* Verbosity level */
extern int g_verbose;
/* VERB1 log("what you sometimes want to see, even on a production box") */
#define VERB1 if (g_verbose >= 1)
/* VERB2 log("debug message, not going into insanely small details") */
#define VERB2 if (g_verbose >= 2)
/* VERB3 log("lots and lots of details") */
#define VERB3 if (g_verbose >= 3)
/* there is no level > 3 */

/* Used for sending dbus signals */
extern CCommLayerServer *g_pCommLayer;

/* Collection of loaded plugins */
extern CPluginManager* g_pPluginManager;

/**
 * A set of blacklisted packages.
 */
extern set_string_t g_setBlackList;

extern pthread_mutex_t g_pJobsMutex;

#endif
