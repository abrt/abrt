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
#include "abrtlib.h"
#include "CrashWatcher.h"
#include <iostream>
#include <sstream>
#include "ABRTException.h"

void CCrashWatcher::Status(const std::string& pMessage, const std::string& pDest)
{
    std::cout << "Update: " + pMessage << std::endl;
    //FIXME: send updates only to job owner
    if(m_pCommLayer != NULL)
       m_pCommLayer->Update(pDest,pMessage);
}

void CCrashWatcher::Warning(const std::string& pMessage, const std::string& pDest)
{
    std::cerr << "Warning: " + pMessage << std::endl;
    if(m_pCommLayer != NULL)
       m_pCommLayer->Warning(pDest,pMessage);
}

void CCrashWatcher::Debug(const std::string& pMessage, const std::string& pDest)
{
    //some logic to add logging levels?
    std::cout << "Debug: " + pMessage << std::endl;
}

CCrashWatcher::CCrashWatcher(const std::string& pPath)
{
    g_cw = this;

    int watch = 0;
    m_sTarget = pPath;

    // TODO: initialize object according parameters -w -d
    // status has to be always created.
    m_pCommLayer = NULL;
    comm_layer_inner_init(this);

    m_pSettings = new CSettings();
    m_pSettings->LoadSettings(std::string(CONF_DIR) + "/abrt.conf");

    m_pMainloop = g_main_loop_new(NULL,FALSE);
    m_pMW = new CMiddleWare(PLUGINS_CONF_DIR,PLUGINS_LIB_DIR);
    if (pthread_mutex_init(&m_pJobsMutex, NULL) != 0)
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CCrashWatcher(): Can't init mutex!");
    }
    try
    {
        SetUpMW();
        SetUpCron();
        FindNewDumps(pPath);
#ifdef ENABLE_DBUS
        m_pCommLayer = new CCommLayerServerDBus();
#elif ENABLE_SOCKET
        m_pCommLayer = new CCommLayerServerSocket();
#endif
//      m_pCommLayer = new CCommLayerServerDBus();
//      m_pCommLayer = new CCommLayerServerSocket();
        m_pCommLayer->Attach(this);

        if ((m_nFd = inotify_init()) == -1)
        {
            throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CCrashWatcher(): Init Failed");
        }
        if ((watch = inotify_add_watch(m_nFd, pPath.c_str(), IN_CREATE)) == -1)
        {
            throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CCrashWatcher(): Add watch failed:" + pPath);
        }
        m_pGio = g_io_channel_unix_new(m_nFd);
    }
    catch (...)
    {
        /* This restores /proc/sys/kernel/core_pattern, among other things */
        delete m_pMW;
        //too? delete m_pCommLayer;
        throw;
    }
}

CCrashWatcher::~CCrashWatcher()
{
    //delete dispatcher, connection, etc..
    //m_pConn->disconnect();

    g_io_channel_unref(m_pGio);
    g_main_loop_unref(m_pMainloop);

    delete m_pCommLayer;
    delete m_pMW;
    delete m_pSettings;
    if (pthread_mutex_destroy(&m_pJobsMutex) != 0)
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CCrashWatcher(): Can't destroy mutex!");
    }
    /* delete pid file */
    unlink(VAR_RUN_PIDFILE);
    /* delete lock file */
    unlink(VAR_RUN_LOCK_FILE);
}

vector_crash_infos_t CCrashWatcher::GetCrashInfos(const std::string &pUID)
{
    vector_crash_infos_t retval;
    Debug("Getting crash infos...");
    try
    {
        vector_pair_string_string_t UUIDsUIDs;
        UUIDsUIDs = m_pMW->GetUUIDsOfCrash(pUID);

        unsigned int ii;
        for (ii = 0; ii < UUIDsUIDs.size(); ii++)
        {
            CMiddleWare::mw_result_t res;
            map_crash_info_t info;

            res = m_pMW->GetCrashInfo(UUIDsUIDs[ii].first, UUIDsUIDs[ii].second, info);
            switch (res)
            {
                case CMiddleWare::MW_OK:
                    retval.push_back(info);
                    break;
                case CMiddleWare::MW_ERROR:
                    Warning("Can not find debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting from database");
                    Status("Can not find debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting from database");
                    m_pMW->DeleteCrashInfo(UUIDsUIDs[ii].first, UUIDsUIDs[ii].second);
                    break;
                case CMiddleWare::MW_FILE_ERROR:
                    {
                        std::string debugDumpDir;
                        Warning("Can not open file in debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting ");
                        Status("Can not open file in debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting ");
                        debugDumpDir = m_pMW->DeleteCrashInfo(UUIDsUIDs[ii].first, UUIDsUIDs[ii].second);
                        m_pMW->DeleteDebugDumpDir(debugDumpDir);
                    }
                    break;
                default:
                    break;
            }
        }
    }
    catch (CABRTException& e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        Warning(e.what());
        Status(e.what());
    }

    //retval = m_pMW->GetCrashInfos(pUID);
    //Notify("Sent crash info");
    return retval;
}

typedef struct thread_data_t {
    pthread_t thread_id;
    char* UUID;
    char* UID;
    char *dest;
} thread_data_t;
static void *create_report(void *arg)
{
    thread_data_t *thread_data = (thread_data_t *) arg;
    map_crash_info_t crashReport;
    g_cw->Debug("Creating report...");
    try
    {
        CMiddleWare::mw_result_t res;
        res = m_pMW->CreateCrashReport(thread_data->UUID, thread_data->UID, crashReport);
        switch (res)
        {
            case CMiddleWare::MW_OK:
                break;
            case CMiddleWare::MW_IN_DB_ERROR:
                g_cw->Warning(std::string("Did not find crash with UUID ")+thread_data->UUID+ " in database.");
                break;
            case CMiddleWare::MW_PLUGIN_ERROR:
                g_cw->Warning(std::string("Particular analyzer plugin isn't loaded or there is an error within plugin(s)."));
                break;
            case CMiddleWare::MW_CORRUPTED:
            case CMiddleWare::MW_FILE_ERROR:
            default:
                {
                    std::string debugDumpDir;
                    g_cw->Warning(std::string("Corrupted crash with UUID ")+thread_data->UUID+", deleting.");
                    debugDumpDir = m_pMW->DeleteCrashInfo(thread_data->UUID, thread_data->UID);
                    m_pMW->DeleteDebugDumpDir(debugDumpDir);
                }
                break;
        }
        /* only one thread can write */
        pthread_mutex_lock(&m_pJobsMutex);
        m_pending_jobs[std::string(thread_data->UID)][thread_data->thread_id] = crashReport;
        pthread_mutex_unlock(&m_pJobsMutex);
        m_pCommLayer->JobDone(thread_data->dest, thread_data->thread_id);
    }
    catch (CABRTException& e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            /* free strduped strings */
            free(thread_data->UUID);
            free(thread_data->UID);
            free(thread_data->dest);
            free(thread_data);
            throw e;
        }
        g_cw->Warning(e.what());
    }
    /* free strduped strings */
    free(thread_data->UUID);
    free(thread_data->UID);
    free(thread_data->dest);
    free(thread_data);

    /* Bogus value. pthreads require us to return void* */
    return NULL;
}
uint64_t CCrashWatcher::CreateReport_t(const std::string &pUUID,const std::string &pUID, const std::string &pSender)
{
    thread_data_t *thread_data = (thread_data_t *)xzalloc(sizeof(thread_data_t));
    if (thread_data != NULL)
    {
        thread_data->UUID = xstrdup(pUUID.c_str());
        thread_data->UID = xstrdup(pUID.c_str());
        thread_data->dest = xstrdup(pSender.c_str());
        if (pthread_create(&(thread_data->thread_id), NULL, create_report, (void *)thread_data) != 0)
        {
            throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CreateReport_t(): Cannot create thread!");
        }
    }
    else
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CreateReport_t(): Cannot allocate memory!");
    }
    //FIXME: we don't use this value anymore, so fix the API
    return 0;
}

CMiddleWare::report_status_t CCrashWatcher::Report(map_crash_report_t pReport, const std::string& pUID)
{
    //#define FIELD(X) crashReport.m_s##X = pReport[#X];
    //crashReport.m_sUUID = pReport["UUID"];
    //ALL_CRASH_REPORT_FIELDS;
    //#undef FIELD
    //for (dbus_map_report_info_t::iterator it = pReport.begin(); it!=pReport.end(); ++it) {
    //     std::cerr << it->second << std::endl;
    //}
    CMiddleWare::report_status_t rs;
    try
    {
        rs = m_pMW->Report(pReport, pUID);
    }
    catch (CABRTException& e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        Warning(e.what());
        Status(e.what());
        return rs;
    }
    return rs;
}

bool CCrashWatcher::DeleteDebugDump(const std::string& pUUID, const std::string& pUID)
{
    try
    {
        std::string debugDumpDir;
        debugDumpDir = m_pMW->DeleteCrashInfo(pUUID,pUID);
        m_pMW->DeleteDebugDumpDir(debugDumpDir);
    }
    catch (CABRTException& e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        Warning(e.what());
        Status(e.what());
        return false;
    }
    return true;
}

map_crash_report_t CCrashWatcher::GetJobResult(uint64_t pJobID, const std::string& pSender)
{
    /* FIXME: once we return the result, we should remove it from map to free memory
       - use some TTL to clean the memory even if client won't get it
       - if we don't find it in the cache we should try to ask MW to get it again??
    */
    return m_pending_jobs[pSender][pJobID];
}

vector_map_string_string_t CCrashWatcher::GetPluginsInfo()
{
    try
    {
        return m_pMW->GetPluginsInfo();
    }
    catch (CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        Warning(e.what());
    }
    // TODO: is it right? I added it just to disable a warning...
    // but maybe returning empty map is wrong here?
    return vector_map_string_string_t();
}

map_plugin_settings_t CCrashWatcher::GetPluginSettings(const std::string& pName, const std::string& pUID)
{
    try
    {
        return m_pMW->GetPluginSettings(pName, pUID);
    }
    catch(CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        Warning(e.what());
    }
}

void CCrashWatcher::RegisterPlugin(const std::string& pName)
{
    try
    {
        m_pMW->RegisterPlugin(pName);
    }
    catch(CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        Warning(e.what());
    }
}

void CCrashWatcher::UnRegisterPlugin(const std::string& pName)
{
    try
    {
        m_pMW->UnRegisterPlugin(pName);
    }
    catch(CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        Warning(e.what());
    }
}

void CCrashWatcher::SetPluginSettings(const std::string& pName, const std::string& pUID, const map_plugin_settings_t& pSettings)
{
    try
    {
        m_pMW->SetPluginSettings(pName, pUID, pSettings);
    }
    catch(CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        Warning(e.what());
    }
}
