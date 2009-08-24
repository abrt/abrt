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
#include <sys/inotify.h>
#include <glib.h>
#include <pthread.h>

#include "ABRTException.h"
#include <iostream>
#include <string>

#include "CrashWatcher.h"


//FIXME: add some struct to be able to join all threads!
typedef struct cron_callback_data_t
{
    std::string m_sPluginName;
    std::string m_sPluginArgs;
    unsigned int m_nTimeout;

    cron_callback_data_t(
                      const std::string& pPluginName,
                      const std::string& pPluginArgs,
                      const unsigned int& pTimeout) :
        m_sPluginName(pPluginName),
        m_sPluginArgs(pPluginArgs),
        m_nTimeout(pTimeout)
    {}
} cron_callback_data_t;


static uint8_t sig_caught; /* = 0 */
static GMainLoop* g_pMainloop;
static CSettings* g_pSettings;

CCrashWatcher *g_cw;
CCommLayerServer *g_pCommLayer;
/*
 * Map to cache the results from CreateReport_t
 * <UID, <UUID, result>>
 */
std::map<const std::string, std::map <int, map_crash_report_t > > g_pending_jobs;
/* mutex to protect g_pending_jobs */
pthread_mutex_t g_pJobsMutex;


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

static void cron_delete_callback_data_cb(gpointer data)
{
    cron_callback_data_t* cronDeleteCallbackData = static_cast<cron_callback_data_t*>(data);
    delete cronDeleteCallbackData;
}

static gboolean cron_activation_periodic_cb(gpointer data)
{
    cron_callback_data_t* cronPeriodicCallbackData = static_cast<cron_callback_data_t*>(data);
    g_cw->Debug("Activating plugin: " + cronPeriodicCallbackData->m_sPluginName);
    RunAction(DEBUG_DUMPS_DIR,
            cronPeriodicCallbackData->m_sPluginName,
            cronPeriodicCallbackData->m_sPluginArgs);
    return TRUE;
}
static gboolean cron_activation_one_cb(gpointer data)
{
    cron_callback_data_t* cronOneCallbackData = static_cast<cron_callback_data_t*>(data);
    g_cw->Debug("Activating plugin: " + cronOneCallbackData->m_sPluginName);
    RunAction(DEBUG_DUMPS_DIR,
            cronOneCallbackData->m_sPluginName,
            cronOneCallbackData->m_sPluginArgs);
    return FALSE;
}
static gboolean cron_activation_reshedule_cb(gpointer data)
{
    cron_callback_data_t* cronResheduleCallbackData = static_cast<cron_callback_data_t*>(data);
    g_cw->Debug("Rescheduling plugin: " + cronResheduleCallbackData->m_sPluginName);
    cron_callback_data_t* cronPeriodicCallbackData = new cron_callback_data_t(cronResheduleCallbackData->m_sPluginName,
                                                                              cronResheduleCallbackData->m_sPluginArgs,
                                                                              cronResheduleCallbackData->m_nTimeout);
    g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                               cronPeriodicCallbackData->m_nTimeout,
                               cron_activation_periodic_cb,
                               static_cast<gpointer>(cronPeriodicCallbackData),
                               cron_delete_callback_data_cb);
    return FALSE;
}

static void SetUpMW()
{
    SetOpenGPGCheck(g_pSettings->GetOpenGPGCheck());
    SetDatabase(g_pSettings->GetDatabase());
    set_strings_t openGPGPublicKeys = g_pSettings->GetOpenGPGPublicKeys();
    set_strings_t::iterator it_k;
    for (it_k = openGPGPublicKeys.begin(); it_k != openGPGPublicKeys.end(); it_k++)
    {
        AddOpenGPGPublicKey(*it_k);
    }
    set_strings_t blackList = g_pSettings->GetBlackList();
    set_strings_t::iterator it_b;
    for (it_b = blackList.begin(); it_b != blackList.end(); it_b++)
    {
        AddBlackListedPackage(*it_b);
    }
    set_strings_t enabledPlugins = g_pSettings->GetEnabledPlugins();
    set_strings_t::iterator it_p;
    for (it_p = enabledPlugins.begin(); it_p != enabledPlugins.end(); it_p++)
    {
        g_pPluginManager->RegisterPlugin(*it_p);
    }
    vector_pair_strings_t actionsAndReporters = g_pSettings->GetActionsAndReporters();
    vector_pair_strings_t::iterator it_ar;
    for (it_ar = actionsAndReporters.begin(); it_ar != actionsAndReporters.end(); it_ar++)
    {
        AddActionOrReporter((*it_ar).first, (*it_ar).second);
    }

    map_analyzer_actions_and_reporters_t analyzerActionsAndReporters = g_pSettings->GetAnalyzerActionsAndReporters();
    map_analyzer_actions_and_reporters_t::iterator it_aar;
    for (it_aar = analyzerActionsAndReporters.begin(); it_aar != analyzerActionsAndReporters.end(); it_aar++)
    {
        vector_pair_strings_t::iterator it_ar;
        for (it_ar = it_aar->second.begin(); it_ar != it_aar->second.end(); it_ar++)
        {
            AddAnalyzerActionOrReporter(it_aar->first, (*it_ar).first, (*it_ar).second);
        }
    }
}

static void SetUpCron()
{
    map_cron_t cron = g_pSettings->GetCron();
    map_cron_t::iterator it_c;
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
            nS = nS <= 0 ? 1 : nS;
            timeout = nS;
        }

        if (nS != -1)
        {
            vector_pair_strings_t::iterator it_ar;
            for (it_ar = it_c->second.begin(); it_ar != it_c->second.end(); it_ar++)
            {
                cron_callback_data_t* cronPeriodicCallbackData = new cron_callback_data_t((*it_ar).first, (*it_ar).second, timeout);
                g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           timeout,
                                           cron_activation_periodic_cb,
                                           static_cast<gpointer>(cronPeriodicCallbackData),
                                           cron_delete_callback_data_cb);
            }
        }
        else
        {
            time_t actTime = time(NULL);
            struct tm locTime;
            localtime_r(&actTime, &locTime);
            locTime.tm_hour = nH;
            locTime.tm_min = nM;
            locTime.tm_sec = 0;
            time_t nextTime = mktime(&locTime);
            if (nextTime == ((time_t)-1))
            {
                throw CABRTException(EXCEP_FATAL, "SetUpCron(): Cannot set up time");
            }
            if (actTime > nextTime)
            {
                timeout = 24*60*60 + (nextTime - actTime);
            }
            else
            {
                timeout = nextTime - actTime;
            }
            vector_pair_strings_t::iterator it_ar;
            for (it_ar = it_c->second.begin(); it_ar != it_c->second.end(); it_ar++)
            {

                cron_callback_data_t* cronOneCallbackData = new cron_callback_data_t((*it_ar).first, (*it_ar).second, timeout);
                g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           timeout,
                                           cron_activation_one_cb,
                                           static_cast<gpointer>(cronOneCallbackData),
                                           cron_delete_callback_data_cb);
                cron_callback_data_t* cronResheduleCallbackData = new cron_callback_data_t((*it_ar).first, (*it_ar).second, 24 * 60 * 60);
                g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
                                           timeout,
                                           cron_activation_reshedule_cb,
                                           static_cast<gpointer>(cronResheduleCallbackData),
                                           cron_delete_callback_data_cb);
            }
        }
    }
}

static void FindNewDumps(const std::string& pPath)
{
    g_cw->Debug("Scanning for unsaved entries...");
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
        throw CABRTException(EXCEP_FATAL, "FindNewDumps(): Couldn't open the directory:" + pPath);
    }

    for (std::vector<std::string>::iterator itt = dirs.begin(); itt != dirs.end(); ++itt){
        map_crash_info_t crashinfo;
        try
        {
            mw_result_t res;
            res = SaveDebugDump(*itt, crashinfo);
            switch (res)
            {
                case MW_OK:
                    g_cw->Debug("Saving into database (" + *itt + ").");
                    RunActionsAndReporters(crashinfo[CD_MWDDD][CD_CONTENT]);
                    break;
                case MW_IN_DB:
                    g_cw->Debug("Already saved in database (" + *itt + ").");
                    break;
                case MW_REPORTED:
                case MW_OCCURED:
                case MW_BLACKLISTED:
                case MW_CORRUPTED:
                case MW_PACKAGE_ERROR:
                case MW_GPG_ERROR:
                case MW_FILE_ERROR:
                default:
                    g_cw->Warning("Corrupted, bad or already saved crash, deleting.");
                    DeleteDebugDumpDir(*itt);
                    break;
            }
        }
        catch (CABRTException& e)
        {
            if (e.type() == EXCEP_FATAL)
            {
                throw e;
            }
            g_cw->Warning(e.what());
        }
    }
}

static void CreatePidFile()
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
    throw CABRTException(EXCEP_FATAL, "can not open pid file");
}

static void Lock()
{
    int lfp = open(VAR_RUN_LOCK_FILE, O_RDWR|O_CREAT, 0640);
    if (lfp < 0)
    {
        throw CABRTException(EXCEP_FATAL, "can not open lock file");
    }
    if (lockf(lfp, F_TLOCK, 0) < 0)
    {
        throw CABRTException(EXCEP_FATAL, "cannot create lock on lockfile");
    }
    /* only first instance continues */
    //sprintf(str,"%d\n",getpid());
    //write(lfp,str,strlen(str)); /* record pid to lockfile */
}

static void handle_fatal_signal(int signal)
{
    sig_caught = signal;
}

/* One of our event sources is sig_caught when it becomes != 0.
 * glib machinery we need to hook it up to the main loop:
 * prepare():
 * If the source can determine that it is ready here (without waiting
 * for the results of the poll() call) it should return TRUE. It can also
 * return a timeout_ value which should be the maximum timeout (in milliseconds)
 * which should be passed to the poll() call.
 * check():
 * Called after all the file descriptors are polled. The source should
 * return TRUE if it is ready to be dispatched.
 * dispatch():
 * Called to dispatch the event source, after it has returned TRUE
 * in either its prepare or its check function. The dispatch function
 * is passed in a callback function and data. The callback function
 * may be NULL if the source was never connected to a callback using
 * g_source_set_callback(). The dispatch function should
 * call the callback function with user_data and whatever additional
 * parameters are needed for this type of event source.
 */
static gboolean waitsignal_prepare(GSource *source, gint *timeout_)
{
    /* We depend on the fact that in Unix, poll() is interrupted
     * by caught signals (in returns EINTR). Thus we do not need to set
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
    g_main_quit(g_pMainloop);
    return 1;
}

/* Inotify handler */
static gboolean handle_event_cb(GIOChannel *gio, GIOCondition condition, gpointer ptr_unused)
{
    GIOError err;
    char *buf = new char[INOTIFY_BUFF_SIZE];
    gsize len;
    gsize i = 0;
    err = g_io_channel_read(gio, buf, INOTIFY_BUFF_SIZE, &len);
    if (err != G_IO_ERROR_NONE)
    {
        g_cw->Warning("Error reading inotify fd.");
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

        g_cw->Debug(std::string("Created file: ") + name);

        /* we want to ignore the lock files */
        if (event->mask & IN_ISDIR)
        {
            if (GetDirSize(DEBUG_DUMPS_DIR) / (1024*1024) < g_pSettings->GetMaxCrashReportsSize())
            {
                //std::string sName = name;
                map_crash_info_t crashinfo;
                try
                {
                    mw_result_t res;
                    res = SaveDebugDump(std::string(DEBUG_DUMPS_DIR) + "/" + name, crashinfo);
                    switch (res)
                    {
                        case MW_OK:
                            g_cw->Debug("New crash, saving...");
                            RunActionsAndReporters(crashinfo[CD_MWDDD][CD_CONTENT]);
                            /* send message to dbus */
                            g_pCommLayer->Crash(crashinfo[CD_PACKAGE][CD_CONTENT]);
                            break;
                        case MW_REPORTED:
                        case MW_OCCURED:
                            /* send message to dbus */
                            g_cw->Debug("Already saved crash, deleting...");
                            g_pCommLayer->Crash(crashinfo[CD_PACKAGE][CD_CONTENT]);
                            DeleteDebugDumpDir(std::string(DEBUG_DUMPS_DIR) + "/" + name);
                            break;
                        case MW_BLACKLISTED:
                        case MW_CORRUPTED:
                        case MW_PACKAGE_ERROR:
                        case MW_GPG_ERROR:
                        case MW_IN_DB:
                        case MW_FILE_ERROR:
                        default:
                            g_cw->Warning("Corrupted or bad crash, deleting...");
                            DeleteDebugDumpDir(std::string(DEBUG_DUMPS_DIR) + "/" + name);
                            break;
                    }
                }
                catch (CABRTException& e)
                {
                    g_cw->Warning(e.what());
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
                g_cw->Debug(std::string("DebugDumps size has exceeded the limit, deleting the last dump: ") + name);
                DeleteDebugDumpDir(std::string(DEBUG_DUMPS_DIR) + "/" + name);
            }
        }
        else
        {
            g_cw->Debug("Some file created, ignoring...");
        }
    }
    delete[] buf;
    return TRUE;
}


int main(int argc, char** argv)
{
    GIOChannel* pGio;
    int daemonize = 0;

    signal(SIGTERM, handle_fatal_signal);
    signal(SIGINT, handle_fatal_signal);

    /* Daemonize unless -d */
    if (!argv[1] || strcmp(argv[1], "-d") != 0)
    {
        daemonize = 1;

        /* Open stdin to /dev/null. We do it before forking
         * in order to emit useful exitcode to the parent
         * if open fails */
        close(STDIN_FILENO);
        xopen("/dev/null", O_RDWR);
        /* forking to background */
        pid_t pid = fork();
        if (pid < 0)
        {
            perror_msg_and_die("can't fork");
        }
        if (pid > 0)
        {
            /* Parent */
            /* Wait for child to notify us via SIGTERM that it feels ok */
            int i = 20; /* 2 sec */
            while (sig_caught == 0 && --i)
            {
                    usleep(100 * 1000);
            }
            _exit(sig_caught != SIGTERM); /* TERM:ok(0), else:bad(1) */
        }
        /* Child (daemon) continues */
        setsid(); /* never fails */
        /* We must not leave fds 0,1,2 closed.
         * Otherwise fprintf(stderr) dumps messages into random fds, etc. */
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        xdup(0);
        xdup(0);
    }

    CCrashWatcher watcher;

    /* Initialization */
    try
    {
        pthread_mutex_init(&g_pJobsMutex, NULL); /* never fails */
        /* DBus init - we want it early so that errors are reported */
        comm_layer_inner_init(&watcher);
        /* Watching DEBUG_DUMPS_DIR for new files... */
        errno = 0;
        int inotify_fd = inotify_init();
        if (inotify_fd == -1)
            perror_msg_and_die("inotify_init failed");
        if (inotify_add_watch(inotify_fd, DEBUG_DUMPS_DIR, IN_CREATE) == -1)
            perror_msg_and_die("inotify_add_watch failed on '%s'", DEBUG_DUMPS_DIR);
        /* (comment here) */
        g_pSettings = new CSettings();
        g_pSettings->LoadSettings(std::string(CONF_DIR) + "/abrt.conf");
        /* (comment here) */
        g_pMainloop = g_main_loop_new(NULL, FALSE);
        /* (comment here) */
        g_pPluginManager = new CPluginManager();
        g_pPluginManager->LoadPlugins();
        SetUpMW();
        SetUpCron();
        FindNewDumps(DEBUG_DUMPS_DIR);
        /* (comment here) */
#ifdef ENABLE_DBUS
        g_pCommLayer = new CCommLayerServerDBus();
#elif ENABLE_SOCKET
        g_pCommLayer = new CCommLayerServerSocket();
#endif
        g_pCommLayer->Attach(&watcher);
        /* (comment here) */
        pGio = g_io_channel_unix_new(inotify_fd);
        g_io_add_watch(pGio, G_IO_IN, handle_event_cb, NULL);
        /* Add an event source which waits for INT/TERM signal */
        GSourceFuncs waitsignal_funcs;
        memset(&waitsignal_funcs, 0, sizeof(waitsignal_funcs));
        waitsignal_funcs.prepare  = waitsignal_prepare;
        waitsignal_funcs.check    = waitsignal_check;
        waitsignal_funcs.dispatch = waitsignal_dispatch;
        /*waitsignal_funcs.finalize = NULL; - already done */
        GSource *waitsignal_src = (GSource*) g_source_new(&waitsignal_funcs, sizeof(*waitsignal_src));
        g_source_attach(waitsignal_src, g_main_context_default());
        /* Mark the territory */
        Lock();
        CreatePidFile();
    }
    catch (...)
    {
        /* Initialization error. Clean up, in reverse order */
        unlink(VAR_RUN_PIDFILE);
        unlink(VAR_RUN_LOCK_FILE);
        g_io_channel_unref(pGio);
        delete g_pCommLayer;
        /* This restores /proc/sys/kernel/core_pattern, among other things: */
        g_pPluginManager->UnLoadPlugins();
        delete g_pPluginManager;

        g_main_loop_unref(g_pMainloop);
        delete g_pSettings;
        if (pthread_mutex_destroy(&g_pJobsMutex) != 0)
        {
            error_msg("threading error: job mutex locked");
        }
        /* Inform parent that initialization failed */
        if (daemonize)
    	    kill(getppid(), SIGINT);
        error_msg_and_die("error while initializing daemon, exiting");
    }

    /* Inform parent that we initialized ok */
    if (daemonize)
        kill(getppid(), SIGTERM);

    /* Enter the event loop */
    try
    {
	watcher.Debug("Running...");
	g_main_run(g_pMainloop);
    }
    catch (CABRTException& e)
    {
        error_msg("error: %s", e.what().c_str());
    }
    catch (std::exception& e)
    {
        error_msg("error: %s", e.what());
    }

    /* Error or INT/TERM. Clean up, in reverse order */
    unlink(VAR_RUN_PIDFILE);
    unlink(VAR_RUN_LOCK_FILE);
    g_io_channel_unref(pGio);
    delete g_pCommLayer;
    /* This restores /proc/sys/kernel/core_pattern, among other things: */
    g_pPluginManager->UnLoadPlugins();
    delete g_pPluginManager;

    g_main_loop_unref(g_pMainloop);
    delete g_pSettings;

    /* Exiting */
    if (sig_caught)
    {
        signal(sig_caught, SIG_DFL);
        raise(sig_caught);
    }
    return 1;
}
