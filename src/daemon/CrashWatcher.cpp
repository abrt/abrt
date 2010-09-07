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
#include "Daemon.h"
#include "abrt_exception.h"
#include "CrashWatcher.h"

void CCrashWatcher::Status(const char *pMessage, const char* peer)
{
    VERB1 log("Update('%s'): %s", peer, pMessage);
    if (g_pCommLayer != NULL)
        g_pCommLayer->Update(pMessage, peer);
}

void CCrashWatcher::Warning(const char *pMessage, const char* peer)
{
    VERB1 log("Warning('%s'): %s", peer, pMessage);
    if (g_pCommLayer != NULL)
        g_pCommLayer->Warning(pMessage, peer);
}

CCrashWatcher::CCrashWatcher()
{
}

CCrashWatcher::~CCrashWatcher()
{
}

vector_map_crash_data_t GetCrashInfos(long caller_uid)
{
    vector_map_crash_data_t retval;
    log("Getting crash infos...");
    try
    {
        vector_string_t crash_ids;
        GetUUIDsOfCrash(caller_uid, crash_ids);

        unsigned int ii;
        for (ii = 0; ii < crash_ids.size(); ii++)
        {
            const char *crash_id = crash_ids[ii].c_str();

            map_crash_data_t info;
            mw_result_t res = FillCrashInfo(crash_id, info);
            switch (res)
            {
                case MW_OK:
                    retval.push_back(info);
                    break;
                case MW_ERROR:
                    error_msg("Dump directory for crash_id %s doesn't exist or misses crucial files, deleting", crash_id);
                    /* Deletes both DB record and dump dir */
                    DeleteDebugDump(crash_id, /*caller_uid:*/ 0);
                    break;
                default:
                    break;
            }
        }
    }
    catch (CABRTException& e)
    {
        error_msg("%s", e.what());
    }

    return retval;
}

/*
 * Called in two cases:
 * (1) by StartJob dbus call -> CreateReportThread(), in the thread
 * (2) by CreateReport dbus call
 * In the second case, it finishes quickly, because previous
 * StartJob dbus call already did all the processing, and we just retrieve
 * the result from dump directory, which is fast.
 */
void CreateReport(const char* crash_id, long caller_uid, int force, map_crash_data_t& crashReport)
{
    /* FIXME: starting from here, any shared data must be protected with a mutex.
     * For example, CreateCrashReport does:
     * g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
     * which is unsafe wrt concurrent updates to g_pPluginManager state.
     */
    mw_result_t res = CreateCrashReport(crash_id, caller_uid, force, crashReport);
    switch (res)
    {
        case MW_OK:
            VERB2 log_map_crash_data(crashReport, "crashReport");
            break;
        case MW_IN_DB_ERROR:
            error_msg("Can't find crash with id %s in database", crash_id);
            break;
        case MW_PLUGIN_ERROR:
            error_msg("Particular analyzer plugin isn't loaded or there is an error within plugin(s)");
            break;
        default:
            error_msg("Corrupted crash with id %s, deleting", crash_id);
            DeleteDebugDump(crash_id, /*caller_uid:*/ 0);
            break;
    }
}

typedef struct thread_data_t {
    pthread_t thread_id;
    long caller_uid;
    int force;
    char* crash_id;
    char* peer;
} thread_data_t;
static void* create_report(void* arg)
{
    thread_data_t *thread_data = (thread_data_t *) arg;

    /* Client name is per-thread, need to set it */
    set_client_name(thread_data->peer);

    try
    {
        log("Creating report...");
        map_crash_data_t crashReport;
        CreateReport(thread_data->crash_id, thread_data->caller_uid, thread_data->force, crashReport);
        g_pCommLayer->JobDone(thread_data->peer);
    }
    catch (CABRTException& e)
    {
        error_msg("%s", e.what());
    }
    catch (...) {}
    set_client_name(NULL);

    /* free strduped strings */
    free(thread_data->crash_id);
    free(thread_data->peer);
    free(thread_data);

    /* Bogus value. pthreads require us to return void* */
    return NULL;
}
int CreateReportThread(const char* crash_id, long caller_uid, int force, const char* pSender)
{
    thread_data_t *thread_data = (thread_data_t *)xzalloc(sizeof(thread_data_t));
    thread_data->crash_id = xstrdup(crash_id);
    thread_data->caller_uid = caller_uid;
    thread_data->force = force;
    thread_data->peer = xstrdup(pSender);

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int r = pthread_create(&thread_data->thread_id, &attr, create_report, thread_data);
    pthread_attr_destroy(&attr);
    if (r != 0)
    {
        free(thread_data->crash_id);
        free(thread_data->peer);
        free(thread_data);
        /* The only reason this may happen is system-wide resource starvation,
         * or ulimit is exceeded (someone floods us with CreateReport() dbus calls?)
         */
        error_msg("Can't create thread");
        return r;
    }
    VERB3 log("Thread %llx created", (unsigned long long)thread_data->thread_id);
    return r;
}


/* Remove dump dir and its DB record */
int DeleteDebugDump(const char *crash_id, long caller_uid)
{
    try
    {
        CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
        database->Connect();
        struct db_row *row = database->GetRow(crash_id);
        if (!row)
        {
            database->DisConnect();
            return ENOENT;
        }

        char caller_uid_str[sizeof(long) * 3 + 2];
        sprintf(caller_uid_str, "%li", caller_uid);

        if (caller_uid != 0 /* not called by root */
         && row->db_inform_all[0] != '1'
         && strcmp(caller_uid_str, row->db_uid) != 0
        ) {
            database->DisConnect();
            db_row_free(row);
            return EPERM;
        }
        database->DeleteRow(crash_id);
        database->DisConnect();
        if (row->db_dump_dir[0] != '\0')
        {
            delete_debug_dump_dir(row->db_dump_dir);
            db_row_free(row);
            return 0; /* success */
        }
        db_row_free(row);
    }
    catch (CABRTException& e)
    {
        error_msg("%s", e.what());
    }
    return EIO; /* generic failure code */
}

void DeleteDebugDump_by_dir(const char *dump_dir)
{
    try
    {
        CDatabase* database = g_pPluginManager->GetDatabase(g_settings_sDatabase.c_str());
        database->Connect();
        database->DeleteRows_by_dir(dump_dir);
        database->DisConnect();

        delete_debug_dump_dir(dump_dir);
    }
    catch (CABRTException& e)
    {
        error_msg("%s", e.what());
    }
}
