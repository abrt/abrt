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

#include <iostream>
#include <sstream>
#include "abrtlib.h"
#include "Daemon.h"
#include "ABRTException.h"
#include "CrashWatcher.h"

void CCrashWatcher::Status(const std::string& pMessage, const std::string& pDest)
{
    log("Update: %s", pMessage.c_str());
    //FIXME: send updates only to job owner
    if (g_pCommLayer != NULL)
        g_pCommLayer->Update(pDest, pMessage);
}

void CCrashWatcher::Warning(const std::string& pMessage)
{
    log("Warning: %s", pMessage.c_str());
    if (g_pCommLayer != NULL)
        g_pCommLayer->Warning(pMessage);
}

void CCrashWatcher::Debug(const std::string& pMessage)
{
    log("Debug: %s", pMessage.c_str());
}

CCrashWatcher::CCrashWatcher()
{
    g_cw = this;
}

CCrashWatcher::~CCrashWatcher()
{
}

vector_crash_infos_t GetCrashInfos(const std::string &pUID)
{
    vector_crash_infos_t retval;
    log("Getting crash infos...");
    try
    {
        vector_pair_string_string_t UUIDsUIDs;
        UUIDsUIDs = GetUUIDsOfCrash(pUID);

        unsigned int ii;
        for (ii = 0; ii < UUIDsUIDs.size(); ii++)
        {
            mw_result_t res;
            map_crash_info_t info;

            res = GetCrashInfo(UUIDsUIDs[ii].first, UUIDsUIDs[ii].second, info);
            switch (res)
            {
                case MW_OK:
                    retval.push_back(info);
                    break;
                case MW_ERROR:
                    g_cw->Warning("Can not find debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting from database");
                    g_cw->Status("Can not find debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting from database");
                    DeleteCrashInfo(UUIDsUIDs[ii].first, UUIDsUIDs[ii].second);
                    break;
                case MW_FILE_ERROR:
                    {
                        std::string debugDumpDir;
                        g_cw->Warning("Can not open file in debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting ");
                        g_cw->Status("Can not open file in debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting ");
                        debugDumpDir = DeleteCrashInfo(UUIDsUIDs[ii].first, UUIDsUIDs[ii].second);
                        DeleteDebugDumpDir(debugDumpDir);
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
        g_cw->Warning(e.what());
        g_cw->Status(e.what());
    }

    //retval = GetCrashInfos(pUID);
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
    log("Creating report...");
    try
    {
        mw_result_t res;
        res = CreateCrashReport(thread_data->UUID, thread_data->UID, crashReport);
        switch (res)
        {
            case MW_OK:
                break;
            case MW_IN_DB_ERROR:
                g_cw->Warning(std::string("Did not find crash with UUID ")+thread_data->UUID+ " in database.");
                break;
            case MW_PLUGIN_ERROR:
                g_cw->Warning(std::string("Particular analyzer plugin isn't loaded or there is an error within plugin(s)."));
                break;
            case MW_CORRUPTED:
            case MW_FILE_ERROR:
            default:
                {
                    std::string debugDumpDir;
                    g_cw->Warning(std::string("Corrupted crash with UUID ")+thread_data->UUID+", deleting.");
                    debugDumpDir = DeleteCrashInfo(thread_data->UUID, thread_data->UID);
                    DeleteDebugDumpDir(debugDumpDir);
                }
                break;
        }
        /* only one thread can write */
        pthread_mutex_lock(&g_pJobsMutex);
        g_pending_jobs[std::string(thread_data->UID)][thread_data->thread_id] = crashReport;
        pthread_mutex_unlock(&g_pJobsMutex);
        g_pCommLayer->JobDone(thread_data->dest, thread_data->thread_id);
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
uint64_t CreateReport_t(const std::string &pUUID,const std::string &pUID, const std::string &pSender)
{
    thread_data_t *thread_data = (thread_data_t *)xzalloc(sizeof(thread_data_t));
    thread_data->UUID = xstrdup(pUUID.c_str());
    thread_data->UID = xstrdup(pUID.c_str());
    thread_data->dest = xstrdup(pSender.c_str());
    if (pthread_create(&(thread_data->thread_id), NULL, create_report, (void *)thread_data) != 0)
    {
        free(thread_data->UUID);
        free(thread_data->UID);
        free(thread_data->dest);
        free(thread_data);
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CreateReport_t(): Cannot create thread!");
    }
    //FIXME: we don't use this value anymore, so fix the API
    return 0;
}

bool DeleteDebugDump(const std::string& pUUID, const std::string& pUID)
{
    try
    {
        std::string debugDumpDir;
        debugDumpDir = DeleteCrashInfo(pUUID, pUID);
        DeleteDebugDumpDir(debugDumpDir);
    }
    catch (CABRTException& e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        g_cw->Warning(e.what());
        g_cw->Status(e.what());
        return false;
    }
    return true;
}

map_crash_report_t GetJobResult(uint64_t pJobID, const std::string& pSender)
{
    /* FIXME: once we return the result, we should remove it from map to free memory
       - use some TTL to clean the memory even if client won't get it
       - if we don't find it in the cache we should try to ask MW to get it again??
    */
    return g_pending_jobs[pSender][pJobID];
}

vector_map_string_string_t GetPluginsInfo()
{
    try
    {
        return g_pPluginManager->GetPluginsInfo();
    }
    catch (CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        g_cw->Warning(e.what());
    }
    // TODO: is it right? I added it just to disable a warning...
    // but maybe returning empty map is wrong here?
    return vector_map_string_string_t();
}

map_plugin_settings_t GetPluginSettings(const std::string& pName, const std::string& pUID)
{
    try
    {
        return g_pPluginManager->GetPluginSettings(pName, pUID);
    }
    catch(CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        g_cw->Warning(e.what());
    }
    // TODO: is it right? I added it just to disable a warning...
    // but maybe returning empty map is wrong here?
    return map_plugin_settings_t();
}

void RegisterPlugin(const std::string& pName)
{
    try
    {
        g_pPluginManager->RegisterPlugin(pName);
    }
    catch(CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        g_cw->Warning(e.what());
    }
}

void UnRegisterPlugin(const std::string& pName)
{
    try
    {
        g_pPluginManager->UnRegisterPlugin(pName);
    }
    catch(CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        g_cw->Warning(e.what());
    }
}

void SetPluginSettings(const std::string& pName, const std::string& pUID, const map_plugin_settings_t& pSettings)
{
    try
    {
        g_pPluginManager->SetPluginSettings(pName, pUID, pSettings);
    }
    catch(CABRTException &e)
    {
        if (e.type() == EXCEP_FATAL)
        {
            throw e;
        }
        g_cw->Warning(e.what());
    }
}
