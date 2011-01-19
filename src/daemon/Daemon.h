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
#include "abrt_crash_data.h"

class CCrashWatcher;
class CCommLayerServer;

/* Used for sending dbus signals */
extern CCommLayerServer *g_pCommLayer;

#endif
