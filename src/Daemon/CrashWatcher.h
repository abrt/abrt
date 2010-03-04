/*
    Copyright (C) 2009  Jiri Moskovcak (jmoskovc@redhat.com)
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
#ifndef CRASHWATCHER_H_
#define CRASHWATCHER_H_

#include <string>
#include <sys/inotify.h>
#include <glib.h>
#include <pthread.h>
#include "MiddleWare.h"
#include "Settings.h"

#ifdef ENABLE_DBUS
    #include "CommLayerServerDBus.h"
#elif ENABLE_SOCKET
    #include "CommLayerServerSocket.h"
#endif
#include "CommLayerInner.h"


class CCrashWatcher
:  public CObserver
{
    public:
        CCrashWatcher();
        virtual ~CCrashWatcher();

    public:
        /* Observer methods */
        virtual void Status(const char *pMessage, const char* peer);
        virtual void Warning(const char *pMessage, const char* peer);
};

vector_map_crash_data_t GetCrashInfos(long caller_uid);
int  CreateReportThread(const char* crash_id, long caller_uid, int force, const char* pSender);
void CreateReport(const char* crash_id, long caller_uid, int force, map_crash_data_t&);
int  DeleteDebugDump(const char *crash_id, long caller_uid);
void DeleteDebugDump_by_dir(const char *dump_dir);

#endif
