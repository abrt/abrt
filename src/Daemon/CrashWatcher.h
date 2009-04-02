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
#include <sys/inotify.h>
#include <glib.h>
//#include "DBusManager.h"
//#include "DBusServerProxy.h"
#include "MiddleWare.h"
#include "Settings.h"

#include "CommLayerServerDBus.h"
#ifdef HAVE_DBUS
    #include "CommLayerServerDBus.h"
#elif HAVE_SOCKET
    #include "CommLayerServerSocket.h"
#endif

// 1024 simultaneous actions
#define INOTIFY_BUFF_SIZE ((sizeof(struct inotify_event)+FILENAME_MAX)*1024)


class CCrashWatcher
//: public CDBusServer_adaptor,
//  public DBus::IntrospectableAdaptor,
//  public DBus::ObjectAdaptor,
:  public CObserver
{
    private:
        static gboolean handle_event_cb(GIOChannel *gio, GIOCondition condition, gpointer data);
        void StartWatch();
        void GStartWatch();
        void Lock();
        void SetUpMW();
        /* finds dumps created when daemon wasn't running */
        void FindNewDumps(const std::string& pPath);

        int m_nFd;
        GIOChannel* m_pGio;
        GMainLoop *m_pMainloop;
        std::string m_sTarget;
        CMiddleWare *m_pMW;
        CCommLayerServer *m_pCommLayer;
        /*FIXME not needed */
        //DBus::Connection *m_pConn;
        CSettings *m_pSettings;
	public:
        //CCrashWatcher(const std::string& pPath,DBus::Connection &connection);
        CCrashWatcher(const std::string& pPath);
		virtual ~CCrashWatcher();
        //run as daemon
        void Daemonize();
        //don't go background - for debug
        void Run();

    /* methods exported on dbus */
    public:
        /*
        vector_crash_infos_t GetCrashInfos(const std::string &pUID);
        dbus_vector_map_crash_infos_t GetCrashInfosMap(const std::string &pDBusSender);
        dbus_map_report_info_t CreateReport(const std::string &pUUID,const std::string &pDBusSender);
        bool Report(dbus_map_report_info_t pReport);
        bool DeleteDebugDump(const std::string& pUUID, const std::string& pDBusSender);
         */
    public:
        /* Observer methods */
        void Update(const std::string&) {}
};

#endif /*CRASHWATCHER_H_*/
