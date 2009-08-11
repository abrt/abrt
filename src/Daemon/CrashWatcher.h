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

class CCrashWatcher
//: public CDBusServer_adaptor,
//  public DBus::IntrospectableAdaptor,
//  public DBus::ObjectAdaptor,
:  public CObserver
{
    private:
        //FIXME: add some struct to be able to join all threads!
        typedef struct SCronCallbackData
        {
            CCrashWatcher* m_pCrashWatcher;
            std::string m_sPluginName;
            std::string m_sPluginArgs;
            unsigned int m_nTimeout;

            SCronCallbackData(CCrashWatcher* pCrashWatcher,
                              const std::string& pPluginName,
                              const std::string& pPluginArgs,
                              const unsigned int& pTimeout) :
                m_pCrashWatcher(pCrashWatcher),
                m_sPluginName(pPluginName),
                m_sPluginArgs(pPluginArgs),
                m_nTimeout(pTimeout)
            {}

        } cron_callback_data_t;

        typedef struct SThreadData {
            pthread_t  thread_id;
            char* UUID;
            char* UID;
            char *dest;
            CCrashWatcher *daemon;
        } thread_data_t;

        /**
         * Map to cache the results from CreateReport_t
         * <UID, <UUID, result>>
         */
        std::map <const std::string, std::map <int, map_crash_report_t > > pending_jobs;
        /**
        * mutex to protect pending_jobs from being accesed by multiple threads at the same time
        */
        pthread_mutex_t m_pJobsMutex;

        static gboolean handle_event_cb(GIOChannel *gio, GIOCondition condition, gpointer data);
        static void *create_report(void *arg);
        static gboolean cron_activation_periodic_cb(gpointer data);
        static gboolean cron_activation_one_cb(gpointer data);
        static gboolean cron_activation_reshedule_cb(gpointer data);
        static void cron_delete_callback_data_cb(gpointer data);

        void StartWatch();
        void GStartWatch();
        void CreatePidFile();
        void Lock();
        void SetUpMW();
        void SetUpCron();
        /* finds dumps created when daemon wasn't running */
        // FIXME: how to catch abrt itself without this?
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
        void Run();
        void StopRun();

    /* methods exported on dbus */
    public:
        virtual vector_crash_infos_t GetCrashInfos(const std::string &pUID);
        /*FIXME: fix CLI and remove this stub*/
        virtual map_crash_report_t CreateReport(const std::string &pUUID,const std::string &pUID){map_crash_report_t retval; return retval;};
        uint64_t CreateReport_t(const std::string &pUUID,const std::string &pUID, const std::string &pSender);
        virtual bool Report(map_crash_report_t pReport, const std::string &pUID);
        virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pUID);
        virtual map_crash_report_t GetJobResult(uint64_t pJobID, const std::string& pSender);
        /* plugins related */
        virtual vector_map_string_string_t GetPluginsInfo();
        virtual map_plugin_settings_t GetPluginSettings(const std::string& pName, const std::string& pUID);
        void RegisterPlugin(const std::string& pName);
        void UnRegisterPlugin(const std::string& pName);

        /* Observer methods */
        void Status(const std::string& pMessage,const std::string& pDest="0");
        void Debug(const std::string& pMessage, const std::string& pDest="0");
        void Warning(const std::string& pMessage, const std::string& pDest="0");
};

#endif /*CRASHWATCHER_H_*/
