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
#include <climits>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <sstream>
#include <cstring>
#include "ABRTException.h"

#define VAR_RUN_LOCK_FILE   VAR_RUN"/abrt.lock"
#define VAR_RUN_PIDFILE     VAR_RUN"/abrt.pid"

/* just a helper function
template< class T >
std::string
to_string( T x )
{
    std::ostringstream o;
    o << x;
    return o.str();
}
*/

/* Is it "." or ".."? */
/* abrtlib candidate */
static bool dot_or_dotdot(const char *filename)
{
    if (filename[0] != '.') return false;
    if (filename[1] == '\0') return true;
    if (filename[1] != '.') return false;
    if (filename[2] == '\0') return true;
    return false;
}

static double GetDirSize(const std::string &pPath);

gboolean CCrashWatcher::handle_event_cb(GIOChannel *gio, GIOCondition condition, gpointer daemon)
{
    GIOError err;
    char *buf = new char[INOTIFY_BUFF_SIZE];
    gsize len;
    gsize i = 0;
    err = g_io_channel_read(gio, buf, INOTIFY_BUFF_SIZE, &len);
    CCrashWatcher *cc = (CCrashWatcher*)daemon;
    if (err != G_IO_ERROR_NONE)
    {
        cc->Warning("Error reading inotify fd.");
        delete[] buf;
        return FALSE;
    }
    /* reconstruct each event and send message to the dbus */
    while (i < len)
    {
        const char *name = NULL;
        struct inotify_event *event;

        event = (struct inotify_event *) &buf[i];
        if (event->len)
            name = &buf[i] + sizeof (struct inotify_event);
        i += sizeof (struct inotify_event) + event->len;

        cc->Debug(std::string("Created file: ") + name);

        /* we want to ignore the lock files */
        if (event->mask & IN_ISDIR)
        {
            if (GetDirSize(DEBUG_DUMPS_DIR) / (1024*1024) < cc->m_pSettings->GetMaxCrashReportsSize())
            {
                //std::string sName = name;
                map_crash_info_t crashinfo;
                try
                {
                    CMiddleWare::mw_result_t res;
                    res = cc->m_pMW->SaveDebugDump(std::string(DEBUG_DUMPS_DIR) + "/" + name, crashinfo);
                    switch (res)
                    {
                        case CMiddleWare::MW_OK:
                            cc->Warning("New crash, saving...");
                            cc->m_pMW->RunActionsAndReporters(crashinfo[CD_MWDDD][CD_CONTENT]);
                            /* send message to dbus */
                            cc->m_pCommLayer->Crash(crashinfo[CD_PACKAGE][CD_CONTENT]);
                            break;
                        case CMiddleWare::MW_REPORTED:
                        case CMiddleWare::MW_OCCURED:
                            /* send message to dbus */
                            cc->Warning("Already saved crash, deleting...");
                            cc->m_pCommLayer->Crash(crashinfo[CD_PACKAGE][CD_CONTENT]);
                            cc->m_pMW->DeleteDebugDumpDir(std::string(DEBUG_DUMPS_DIR) + "/" + name);
                            break;
                        case CMiddleWare::MW_BLACKLISTED:
                        case CMiddleWare::MW_CORRUPTED:
                        case CMiddleWare::MW_PACKAGE_ERROR:
                        case CMiddleWare::MW_GPG_ERROR:
                        case CMiddleWare::MW_IN_DB:
                        case CMiddleWare::MW_FILE_ERROR:
                        default:
                            cc->Warning("Corrupted or bad crash, deleting...");
                            cc->m_pMW->DeleteDebugDumpDir(std::string(DEBUG_DUMPS_DIR) + "/" + name);
                            break;
                    }
                }
                catch (CABRTException& e)
                {
                    cc->Warning(e.what());
                    if (e.type() == EXCEP_FATAL)
                    {
                        delete[] buf;
                        return -1;
                    }
                }
                catch (...)
                {
                    delete[] buf;
                    throw;
                }
            }
            else
            {
                cc->Debug(std::string("DebugDumps size has exceeded the limit, deleting the last dump: ") + name);
                cc->m_pMW->DeleteDebugDumpDir(std::string(DEBUG_DUMPS_DIR) + "/" + name);
            }
        }
        else
        {
            cc->Debug("Some file created, ignoring...");
        }
    }
    delete[] buf;
    return TRUE;
}

void *CCrashWatcher::create_report(void *arg){
    thread_data_t *thread_data = (thread_data_t *) arg;
    map_crash_info_t crashReport;
    thread_data->daemon->Debug("Creating report...");
    try
    {
        CMiddleWare::mw_result_t res;
        res = thread_data->daemon->m_pMW->CreateCrashReport(thread_data->UUID,thread_data->UID,crashReport);
        switch (res)
        {
            case CMiddleWare::MW_OK:
                break;
            case CMiddleWare::MW_IN_DB_ERROR:
                thread_data->daemon->Warning(std::string("Did not find crash with UUID ")+thread_data->UUID+ " in database.");
                break;
            case CMiddleWare::MW_CORRUPTED:
            case CMiddleWare::MW_FILE_ERROR:
            default:
                {
                    std::string debugDumpDir;
                    thread_data->daemon->Warning(std::string("Corrupted crash with UUID ")+thread_data->UUID+", deleting.");
                    debugDumpDir = thread_data->daemon->m_pMW->DeleteCrashInfo(thread_data->UUID, thread_data->UID);
                    thread_data->daemon->m_pMW->DeleteDebugDumpDir(debugDumpDir);
                }
                break;
        }
        /* only one thread can write */
        pthread_mutex_lock(&(thread_data->daemon->m_pJobsMutex));
        thread_data->daemon->pending_jobs[std::string(thread_data->UID)][thread_data->thread_id] = crashReport;
        pthread_mutex_unlock(&(thread_data->daemon->m_pJobsMutex));
        thread_data->daemon->m_pCommLayer->JobDone(thread_data->dest, thread_data->thread_id);
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
        thread_data->daemon->Warning(e.what());
    }
    /* free strduped strings */
    free(thread_data->UUID);
    free(thread_data->UID);
    free(thread_data->dest);
    free(thread_data);
}

gboolean CCrashWatcher::cron_activation_periodic_cb(gpointer data)
{
    cron_callback_data_t* cronPeriodicCallbackData = static_cast<cron_callback_data_t*>(data);
    cronPeriodicCallbackData->m_pCrashWatcher->Debug("Activating plugin: " + cronPeriodicCallbackData->m_sPluginName);
    cronPeriodicCallbackData->m_pCrashWatcher->m_pMW->RunAction(cronPeriodicCallbackData->m_pCrashWatcher->m_sTarget,
                                                                cronPeriodicCallbackData->m_sPluginName,
                                                                cronPeriodicCallbackData->m_sPluginArgs);
    return TRUE;
}
gboolean CCrashWatcher::cron_activation_one_cb(gpointer data)
{
    cron_callback_data_t* cronOneCallbackData = static_cast<cron_callback_data_t*>(data);
    cronOneCallbackData->m_pCrashWatcher->Debug("Activating plugin: " + cronOneCallbackData->m_sPluginName);
    cronOneCallbackData->m_pCrashWatcher->m_pMW->RunAction(cronOneCallbackData->m_pCrashWatcher->m_sTarget,
                                                           cronOneCallbackData->m_sPluginName,
                                                           cronOneCallbackData->m_sPluginArgs);
    return FALSE;
}
gboolean CCrashWatcher::cron_activation_reshedule_cb(gpointer data)
{
    cron_callback_data_t* cronResheduleCallbackData = static_cast<cron_callback_data_t*>(data);
    cronResheduleCallbackData->m_pCrashWatcher->Debug("Rescheduling plugin: " + cronResheduleCallbackData->m_sPluginName);
    cron_callback_data_t* cronPeriodicCallbackData = new cron_callback_data_t(cronResheduleCallbackData->m_pCrashWatcher,
                                                                              cronResheduleCallbackData->m_sPluginName,
                                                                              cronResheduleCallbackData->m_sPluginArgs,
                                                                              cronResheduleCallbackData->m_nTimeout);
    g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                               cronPeriodicCallbackData->m_nTimeout,
                               cron_activation_periodic_cb,
                               static_cast<gpointer>(cronPeriodicCallbackData),
                               cron_delete_callback_data_cb);


    return FALSE;
}

void CCrashWatcher::cron_delete_callback_data_cb(gpointer data)
{
    cron_callback_data_t* cronDeleteCallbackData = static_cast<cron_callback_data_t*>(data);
    delete cronDeleteCallbackData;
}

void CCrashWatcher::SetUpMW()
{
    m_pMW->SetOpenGPGCheck(m_pSettings->GetOpenGPGCheck());
    m_pMW->SetDatabase(m_pSettings->GetDatabase());
    CSettings::set_strings_t openGPGPublicKeys = m_pSettings->GetOpenGPGPublicKeys();
    CSettings::set_strings_t::iterator it_k;
    for (it_k = openGPGPublicKeys.begin(); it_k != openGPGPublicKeys.end(); it_k++)
    {
        m_pMW->AddOpenGPGPublicKey(*it_k);
    }
    CSettings::set_strings_t blackList = m_pSettings->GetBlackList();
    CSettings::set_strings_t::iterator it_b;
    for (it_b = blackList.begin(); it_b != blackList.end(); it_b++)
    {
        m_pMW->AddBlackListedPackage(*it_b);
    }
    CSettings::set_strings_t enabledPlugins = m_pSettings->GetEnabledPlugins();
    CSettings::set_strings_t::iterator it_p;
    for (it_p = enabledPlugins.begin(); it_p != enabledPlugins.end(); it_p++)
    {
        m_pMW->RegisterPlugin(*it_p);
    }
    CSettings::vector_pair_strings_t actionsAndReporters = m_pSettings->GetActionsAndReporters();
    CSettings::vector_pair_strings_t::iterator it_ar;
    for (it_ar = actionsAndReporters.begin(); it_ar != actionsAndReporters.end(); it_ar++)
    {
        m_pMW->AddActionOrReporter((*it_ar).first, (*it_ar).second);
    }

    CSettings::map_analyzer_actions_and_reporters_t analyzerActionsAndReporters = m_pSettings->GetAnalyzerActionsAndReporters();
    CSettings::map_analyzer_actions_and_reporters_t::iterator it_aar;
    for (it_aar = analyzerActionsAndReporters.begin(); it_aar != analyzerActionsAndReporters.end(); it_aar++)
    {
        CSettings::vector_pair_strings_t::iterator it_ar;
        for (it_ar = it_aar->second.begin(); it_ar != it_aar->second.end(); it_ar++)
        {
            m_pMW->AddAnalyzerActionOrReporter(it_aar->first, (*it_ar).first, (*it_ar).second);
        }
    }
}

void CCrashWatcher::SetUpCron()
{
    CSettings::map_cron_t cron = m_pSettings->GetCron();
    CSettings::map_cron_t::iterator it_c;
    for (it_c = cron.begin(); it_c != cron.end(); it_c++)
    {
        std::string::size_type pos = it_c->first.find(":");
        int timeout = 0;
        int nH = -1;
        int nM = -1;
        int nS = -1;

        if (pos != std::string::npos)
        {
            std::string sH = "";
            std::string sM = "";

            sH = it_c->first.substr(0, pos);
            nH = atoi(sH.c_str());
            nH = nH > 23 ? 23 : nH;
            nH = nH < 0 ? 0 : nH;
            nM = nM > 59 ? 59 : nM;
            nM = nM < 0 ? 0 : nM;
            timeout += nH * 60 * 60;
            sM = it_c->first.substr(pos + 1);
            nM = atoi(sM.c_str());
            timeout += nM * 60;
        }
        else
        {
            std::string sS = "";

            sS = it_c->first;
            nS = atoi(sS.c_str());
            nS = nS < 0 ? 0 : nS;
            timeout = nS;
        }

        if (nS != -1)
        {
            CSettings::vector_pair_strings_t::iterator it_ar;
            for (it_ar = it_c->second.begin(); it_ar != it_c->second.end(); it_ar++)
            {

                cron_callback_data_t* cronPeriodicCallbackData = new cron_callback_data_t(this, (*it_ar).first, (*it_ar).second, timeout);
                g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           timeout ,
                                           cron_activation_periodic_cb,
                                           static_cast<gpointer>(cronPeriodicCallbackData),
                                           cron_delete_callback_data_cb);
            }
        }
        else
        {
            time_t actTime = time(NULL);
            if (actTime == ((time_t)-1))
            {
                throw CABRTException(EXCEP_FATAL, "CCrashWatcher::SetUpCron(): Cannot get time.");
            }
            struct tm locTime;
            if (localtime_r(&actTime, &locTime) == NULL)
            {
                throw CABRTException(EXCEP_FATAL, "CCrashWatcher::SetUpCron(): Cannot get local time.");
            }
            locTime.tm_hour = nH;
            locTime.tm_min = nM;
            locTime.tm_sec = 0;
            time_t nextTime = mktime(&locTime);
            if (nextTime == ((time_t)-1))
            {
                throw CABRTException(EXCEP_FATAL, "CCrashWatcher::SetUpCron(): Cannot set up time.");
            }
            if (actTime > nextTime)
            {
                timeout = 24*60*60 + (nextTime - actTime);
            }
            else
            {
                timeout = nextTime - actTime;
            }
            CSettings::vector_pair_strings_t::iterator it_ar;
            for (it_ar = it_c->second.begin(); it_ar != it_c->second.end(); it_ar++)
            {

                cron_callback_data_t* cronOneCallbackData = new cron_callback_data_t(this, (*it_ar).first, (*it_ar).second, timeout);
                g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           timeout,
                                           cron_activation_one_cb,
                                           static_cast<gpointer>(cronOneCallbackData),
                                           cron_delete_callback_data_cb);
                cron_callback_data_t* cronResheduleCallbackData = new cron_callback_data_t(this, (*it_ar).first, (*it_ar).second, 24 * 60 * 60);
                g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           timeout,
                                           cron_activation_reshedule_cb,
                                           static_cast<gpointer>(cronResheduleCallbackData),
                                           cron_delete_callback_data_cb);
            }
        }
    }
}

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

static double GetDirSize(const std::string &pPath)
{
    double size = 0;
    struct dirent *ep;
    struct stat stats;
    DIR *dp;

    dp = opendir(pPath.c_str());
    if (dp != NULL)
    {
        while ((ep = readdir(dp)) != NULL)
        {
            if (dot_or_dotdot(ep->d_name))
                continue;
            std::string dname = pPath + "/" + ep->d_name;
            if (lstat(dname.c_str(), &stats) == 0)
            {
                if (S_ISDIR(stats.st_mode))
                {
                    size += GetDirSize(dname);
                }
                else if (S_ISREG(stats.st_mode))
                {
                    size += stats.st_size;
                }
            }
        }
        closedir(dp);
    }
    else
    {
        throw CABRTException(EXCEP_FATAL, std::string(__func__) + ": Init Failed");
    }
    return size;
}

CCrashWatcher::CCrashWatcher(const std::string& pPath)
{
    int watch = 0;
    m_sTarget = pPath;

    // TODO: initialize object according parameters -w -d
    // status has to be always created.
    m_pCommLayer = NULL;
    m_pCommLayerInner = new CCommLayerInner(this, true, true);
    comm_layer_inner_init(m_pCommLayerInner);

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
    delete m_pCommLayerInner;
    if (pthread_mutex_destroy(&m_pJobsMutex) != 0)
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CCrashWatcher(): Can't destroy mutex!");
    }
    /* delete pid file */
    unlink(VAR_RUN_PIDFILE);
    /* delete lock file */
    unlink(VAR_RUN_LOCK_FILE);
}
void CCrashWatcher::FindNewDumps(const std::string& pPath)
{
    Debug("Scanning for unsaved entries...");
    struct dirent *ep;
    struct stat stats;
    DIR *dp;
    std::vector<std::string> dirs;
    std::string dname;
    // get potential unsaved debugdumps
    dp = opendir(pPath.c_str());
    if (dp != NULL)
    {
        while ((ep = readdir(dp)))
        {
            if (dot_or_dotdot(ep->d_name))
                continue;
            dname = pPath + "/" + ep->d_name;
            if (lstat(dname.c_str(), &stats) == 0)
            {
                if (S_ISDIR(stats.st_mode))
                {
                    dirs.push_back(dname);
                }
            }
        }
        (void) closedir(dp);
    }
    else
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::FindNewDumps(): Couldn't open the directory:" + pPath);
    }

    for (std::vector<std::string>::iterator itt = dirs.begin(); itt != dirs.end(); ++itt){
        map_crash_info_t crashinfo;
        try
        {
            CMiddleWare::mw_result_t res;
            res = m_pMW->SaveDebugDump(*itt, crashinfo);
            switch (res)
            {
                case CMiddleWare::MW_OK:
                    Debug("Saving into database (" + *itt + ").");
                    m_pMW->RunActionsAndReporters(crashinfo[CD_MWDDD][CD_CONTENT]);
                    break;
                case CMiddleWare::MW_IN_DB:
                    Debug("Already saved in database (" + *itt + ").");
                    break;
                case CMiddleWare::MW_REPORTED:
                case CMiddleWare::MW_OCCURED:
                case CMiddleWare::MW_BLACKLISTED:
                case CMiddleWare::MW_CORRUPTED:
                case CMiddleWare::MW_PACKAGE_ERROR:
                case CMiddleWare::MW_GPG_ERROR:
                case CMiddleWare::MW_FILE_ERROR:
                default:
                    Warning("Corrupted, bad or already saved crash, deleting.");
                    m_pMW->DeleteDebugDumpDir(*itt);
                    break;
            }
        }
        catch (CABRTException& e)
        {
            if (e.type() == EXCEP_FATAL)
            {
                throw e;
            }
            Warning(e.what());
        }
    }
}
void CCrashWatcher::CreatePidFile()
{
    int fd;

    /* JIC */
    unlink(VAR_RUN_PIDFILE);

    /* open the pidfile */
    fd = open(VAR_RUN_PIDFILE, O_WRONLY|O_CREAT|O_EXCL, 0644);
    if (fd >= 0)
    {
        /* write our pid to it */
        char buf[sizeof(int)*3 + 2];
        int len = sprintf(buf, "%u\n", (unsigned)getpid());
        write(fd, buf, len);
        close(fd);
        return;
    }

    /* something went wrong */
    throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CreatePidFile(): can not open pid file");
}

void CCrashWatcher::Lock()
{
    int lfp = open(VAR_RUN_LOCK_FILE, O_RDWR|O_CREAT, 0640);
    if (lfp < 0)
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::Lock(): can not open lock file");
    }
    if (lockf(lfp, F_TLOCK, 0) < 0)
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::Lock(): cannot create lock on lockfile");
    }
    /* only first instance continues */
    //sprintf(str,"%d\n",getpid());
    //write(lfp,str,strlen(str)); /* record pid to lockfile */
}

void CCrashWatcher::StartWatch()
{
    char *buff = new char[INOTIFY_BUFF_SIZE];
    int len = 0;
    int i = 0;
    char action[FILENAME_MAX];
    struct inotify_event *pevent;
    //run forever
    while (1)
    {
        i = 0;
        len = read(m_nFd,buff,INOTIFY_BUFF_SIZE);
        while (i < len)
        {
            pevent = (struct inotify_event *)&buff[i];
            if (pevent->len)
            {
                std::strcpy(action, pevent->name);
            }
            else
            {
                std::strcpy(action, m_sTarget.c_str());
            }

            i += sizeof(struct inotify_event) + pevent->len;
            Debug(std::string("Created file: ") + action);
        }
    }
    delete[] buff;
}

extern uint8_t sig_caught;
//prepare()
//If the source can determine that it is ready here (without waiting
//for the results of the poll() call) it should return TRUE. It can also
//return a timeout_ value which should be the maximum timeout (in milliseconds)
//which should be passed to the poll() call.
//check()
//Called after all the file descriptors are polled. The source should
//return TRUE if it is ready to be dispatched.
//dispatch()
//Called to dispatch the event source, after it has returned TRUE
//in either its prepare or its check function. The dispatch function
//is passed in a callback function and data. The callback function
//may be NULL if the source was never connected to a callback using
//g_source_set_callback(). The dispatch function should
//call the callback function with user_data and whatever additional
//parameters are needed for this type of event source.
typedef struct SignalSource
{
    GSource src;
    CCrashWatcher* watcher;
} SignalSource;
static gboolean waitsignal_prepare(GSource *source, gint *timeout_)
{
    /* We depend on the fact that in Unix, poll() is interrupted
     * by caught signals (returns EINTR). Thus we do not need to set
     * a small timeout here: infinite timeout (-1) works too */
    *timeout_ = -1;
    return sig_caught != 0;
}
static gboolean waitsignal_check(GSource *source)
{
    return sig_caught != 0;
}
static gboolean waitsignal_dispatch(GSource *source, GSourceFunc callback, gpointer user_data)
{
    SignalSource *ssrc = (SignalSource*) source;
    ssrc->watcher->StopRun();
    return 1;
}

/* daemon loop with glib */
void CCrashWatcher::GStartWatch()
{
    g_io_add_watch(m_pGio, G_IO_IN, handle_event_cb, this);

    GSourceFuncs waitsignal_funcs;
    memset(&waitsignal_funcs, 0, sizeof(waitsignal_funcs));
    waitsignal_funcs.prepare  = waitsignal_prepare;
    waitsignal_funcs.check    = waitsignal_check;
    waitsignal_funcs.dispatch = waitsignal_dispatch;
    //waitsignal_funcs.finalize = NULL; - already done
    SignalSource *waitsignal_src = (SignalSource*) g_source_new(&waitsignal_funcs, sizeof(*waitsignal_src));
    waitsignal_src->watcher = this;
    g_source_attach(&waitsignal_src->src, g_main_context_default());

    //enter the event loop
    g_main_run(m_pMainloop);
}

void CCrashWatcher::Run()
{
    Debug("Running...");
    Lock();
    CreatePidFile();
    GStartWatch();
}

void CCrashWatcher::StopRun()
{
    g_main_quit(m_pMainloop);
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

uint64_t CCrashWatcher::CreateReport_t(const std::string &pUUID,const std::string &pUID, const std::string &pSender)
{
    thread_data_t *thread_data = (thread_data_t *)xzalloc(sizeof(thread_data_t));
    if (thread_data != NULL)
    {
        thread_data->UUID = xstrdup(pUUID.c_str());
        thread_data->UID = xstrdup(pUID.c_str());
        thread_data->dest = xstrdup(pSender.c_str());
        thread_data->daemon = this;
        if(pthread_create(&(thread_data->thread_id), NULL, create_report, (void *)thread_data) != 0)
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

bool CCrashWatcher::Report(map_crash_report_t pReport, const std::string& pUID)
{
    //#define FIELD(X) crashReport.m_s##X = pReport[#X];
    //crashReport.m_sUUID = pReport["UUID"];
    //ALL_CRASH_REPORT_FIELDS;
    //#undef FIELD
    //for (dbus_map_report_info_t::iterator it = pReport.begin(); it!=pReport.end(); ++it) {
    //     std::cerr << it->second << std::endl;
    //}
    try
    {
        struct passwd* pw = getpwuid(atoi(pUID.c_str()));
        std::string home = pw ? pw->pw_dir : "";
        if (home != "")
        {
            m_pMW->Report(pReport, home + "/.abrt/");
        }
        else
        {
            m_pMW->Report(pReport);
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
        return false;
    }
    return true;
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
    return pending_jobs[pSender][pJobID];
}

vector_map_string_string_t CCrashWatcher::GetPluginsInfo()
{
    try
    {
        return m_pMW->GetPluginsInfo();
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
