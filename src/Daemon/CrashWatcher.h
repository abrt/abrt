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
#include <pthread.h>
//#include "DBusManager.h"
//#include "DBusServerProxy.h"
#include "MiddleWare.h"
#include "Settings.h"

//FIXME remove when it gets to autoconf
//#include "CommLayerServerDBus.h"
//#include "CommLayerServerSocket.h"
#ifdef ENABLE_DBUS
    #include "CommLayerServerDBus.h"
#elif ENABLE_SOCKET
    #include "CommLayerServerSocket.h"
#endif
#include "CommLayerInner.h"

// 1024 simultaneous actions
#define INOTIFY_BUFF_SIZE ((sizeof(struct inotify_event)+FILENAME_MAX)*1024)

#define VAR_RUN_LOCK_FILE   VAR_RUN"/abrt.lock"
#define VAR_RUN_PIDFILE     VAR_RUN"/abrt.pid"


class CCrashWatcher
//: public CDBusServer_adaptor,
//  public DBus::IntrospectableAdaptor,
//  public DBus::ObjectAdaptor,
:  public CObserver
{

    public:
        //CCrashWatcher(const std::string& pPath,DBus::Connection &connection);
        CCrashWatcher(const std::string& pPath);
        virtual ~CCrashWatcher();

    public:
        /* Observer methods */
        virtual void Status(const std::string& pMessage,const std::string& pDest="0");
        virtual void Debug(const std::string& pMessage, const std::string& pDest="0");
        virtual void Warning(const std::string& pMessage, const std::string& pDest="0");
        virtual vector_crash_infos_t GetCrashInfos(const std::string &pUID);
        /*FIXME: fix CLI and remove this stub*/
        virtual map_crash_report_t CreateReport(const std::string &pUUID,const std::string &pUID)
        {
            map_crash_report_t retval;
            return retval;
        }
        uint64_t CreateReport_t(const std::string &pUUID,const std::string &pUID, const std::string &pSender);
        virtual CMiddleWare::report_status_t Report(map_crash_report_t pReport, const std::string &pUID);
        virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pUID);
        virtual map_crash_report_t GetJobResult(uint64_t pJobID, const std::string& pSender);
        /* plugins related */
        virtual vector_map_string_string_t GetPluginsInfo();
        virtual map_plugin_settings_t GetPluginSettings(const std::string& pName, const std::string& pUID);
        virtual void SetPluginSettings(const std::string& pName, const std::string& pUID, const map_plugin_settings_t& pSettings);
        virtual void RegisterPlugin(const std::string& pName);
        virtual void UnRegisterPlugin(const std::string& pName);
};


extern CCrashWatcher *g_cw;
extern int m_nFd;
extern GIOChannel* m_pGio;
extern GMainLoop *m_pMainloop;
extern std::string m_sTarget;
extern CMiddleWare *m_pMW;
extern CCommLayerServer *m_pCommLayer;
extern CSettings *m_pSettings;
/**
 * Map to cache the results from CreateReport_t
 * <UID, <UUID, result>>
 */
extern std::map<const std::string, std::map <int, map_crash_report_t > > m_pending_jobs;
/**
* mutex to protect m_pending_jobs from being accesed by multiple threads at the same time
*/
extern pthread_mutex_t m_pJobsMutex;


//extern gboolean handle_event_cb(GIOChannel *gio, GIOCondition condition, gpointer data);
//extern void *create_report(void *arg);
//extern gboolean cron_activation_periodic_cb(gpointer data);
//extern gboolean cron_activation_one_cb(gpointer data);
//extern gboolean cron_activation_reshedule_cb(gpointer data);
//extern void cron_delete_callback_data_cb(gpointer data);
//extern void CreatePidFile();
//extern void Lock();
extern void SetUpMW();
extern void SetUpCron();
/* finds dumps created when daemon wasn't running */
// FIXME: how to catch abrt itself without this?
extern void FindNewDumps(const std::string& pPath);

#endif /*CRASHWATCHER_H_*/
