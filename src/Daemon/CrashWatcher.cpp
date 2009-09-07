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

void CCrashWatcher::Status(const std::string& pMessage, uint64_t pJobID)
{
    log("Update: %s", pMessage.c_str());
    //FIXME: send updates only to job owner
    if (g_pCommLayer != NULL)
        g_pCommLayer->Update(pMessage, pJobID);
}

void CCrashWatcher::Warning(const std::string& pMessage, uint64_t pJobID)
{
    log("Warning: %s", pMessage.c_str());
    if (g_pCommLayer != NULL)
        g_pCommLayer->Warning(pMessage, pJobID);
}

CCrashWatcher::CCrashWatcher()
{
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
                    warn_client("Can not find debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting from database");
                    update_client("Can not find debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting from database");
                    DeleteCrashInfo(UUIDsUIDs[ii].first, UUIDsUIDs[ii].second);
                    break;
                case MW_FILE_ERROR:
                    {
                        std::string debugDumpDir;
                        warn_client("Can not open file in debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting");
                        update_client("Can not open file in debug dump directory for UUID: " + UUIDsUIDs[ii].first + ", deleting");
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
        warn_client(e.what());
        update_client(e.what());
    }

    //retval = GetCrashInfos(pUID);
    //Notify("Sent crash info");
    return retval;
}

typedef struct thread_data_t {
    pthread_t thread_id;
    char* UUID;
    char* UID;
    char* dest;
} thread_data_t;
static void *create_report(void *arg)
{
    thread_data_t *thread_data = (thread_data_t *) arg;
    map_crash_info_t crashReport;

    g_pCommLayer->JobStarted(thread_data->dest, uint64_t(thread_data->thread_id));

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
                warn_client(std::string("Did not find crash with UUID ") + thread_data->UUID + " in database");
                break;
            case MW_PLUGIN_ERROR:
                warn_client(std::string("Particular analyzer plugin isn't loaded or there is an error within plugin(s)"));
                break;
            case MW_CORRUPTED:
            case MW_FILE_ERROR:
            default:
                warn_client(std::string("Corrupted crash with UUID ") + thread_data->UUID + ", deleting");
                std::string debugDumpDir = DeleteCrashInfo(thread_data->UUID, thread_data->UID);
                DeleteDebugDumpDir(debugDumpDir);
                break;
        }
        /* only one thread can write */
        pthread_mutex_lock(&g_pJobsMutex);
        g_pending_jobs[std::string(thread_data->UID)][uint64_t(thread_data->thread_id)] = crashReport;
        pthread_mutex_unlock(&g_pJobsMutex);
        g_pCommLayer->JobDone(thread_data->dest, uint64_t(thread_data->thread_id));
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
        warn_client(e.what());
    }
    /* free strduped strings */
    free(thread_data->UUID);
    free(thread_data->UID);
    free(thread_data->dest);
    free(thread_data);

    /* Bogus value. pthreads require us to return void* */
    return NULL;
}
uint64_t CreateReport_t(const char* pUUID, const char* pUID, const char* pSender)
{
    thread_data_t *thread_data = (thread_data_t *)xzalloc(sizeof(thread_data_t));
    thread_data->UUID = xstrdup(pUUID);
    thread_data->UID = xstrdup(pUID);
    thread_data->dest = xstrdup(pSender);
    if (pthread_create(&thread_data->thread_id, NULL, create_report, (void *)thread_data) != 0)
    {
        free(thread_data->UUID);
        free(thread_data->UID);
        free(thread_data->dest);
        free(thread_data);
        /* The only reason this may happen is system-wide resource starvation,
         * or ulimit is exceeded (someoune floods us with CreateReport() dbus calls?)
         */
        error_msg("cannot create thread");
        return 0;
    }
    return uint64_t(thread_data->thread_id);
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
        warn_client(e.what());
        update_client(e.what());
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
