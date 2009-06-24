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

#include "CrashWatcher.h"
#include <unistd.h>
#include <iostream>
#include <climits>
#include <cstdlib>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <csignal>
#include <sstream>
#include <dirent.h>
#include <cstring>
#include "ABRTException.h"

#define VAR_RUN_LOCK_FILE   VAR_RUN"/abrt.lock"
#define VAR_RUN_PIDFILE   VAR_RUN"/abrt.pid"

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

gboolean CCrashWatcher::handle_event_cb(GIOChannel *gio, GIOCondition condition, gpointer daemon){
    GIOError err;
    char *buf = new char[INOTIFY_BUFF_SIZE];
    gsize len;
    gsize i = 0;
    err = g_io_channel_read (gio, buf, INOTIFY_BUFF_SIZE, &len);
    CCrashWatcher *cc = (CCrashWatcher*)daemon;
    if (err != G_IO_ERROR_NONE)
    {
            cc->Warning("Error reading inotify fd.");
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
        if(event->mask & IN_ISDIR)
        {
            if(cc->GetDirSize(DEBUG_DUMPS_DIR)/(1024*1024.0) < cc->m_pSettings->GetMaxCrashReportsSize()){
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
                        return -1;
                    }
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

void CCrashWatcher::Status(const std::string& pMessage)
{
    std::cout << "Update: " + pMessage << std::endl;
}

void CCrashWatcher::Warning(const std::string& pMessage)
{
    std::cerr << "Warning: " + pMessage << std::endl;
}

void CCrashWatcher::Debug(const std::string& pMessage)
{
    //some logic to add logging levels?
    std::cout << "Debug: " + pMessage << std::endl;
}

double CCrashWatcher::GetDirSize(const std::string &pPath)
{
    double size = 0;
    int stat(const char *path, struct stat *buf);
    struct dirent *ep;
    struct stat stats;
    DIR *dp;
    std::string dname;
    dp = opendir (pPath.c_str());
    if (dp != NULL)
    {
        while ((ep = readdir (dp))){
            if(strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0){
                dname = pPath + "/" + ep->d_name;
                lstat (dname.c_str(), &stats);
                if(S_ISDIR (stats.st_mode)){
                    size += GetDirSize(dname);
                }
                else if(S_ISREG(stats.st_mode)){
                    size += stats.st_size;
                }
            }
        }
        (void) closedir (dp);
    }
    else
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::GetDirSize(): Init Failed");
    }
    return size;
}

CCrashWatcher::CCrashWatcher(const std::string& pPath)
{
    int watch = 0;
    m_sTarget = pPath;

    // TODO: initialize object according parameters -w -d
    // status has to be always created.
    m_pCommLayerInner = new CCommLayerInner(this, true, true);
    comm_layer_inner_init(m_pCommLayerInner);

    m_pSettings = new CSettings();
    m_pSettings->LoadSettings(std::string(CONF_DIR) + "/abrt.conf");

    m_pMainloop = g_main_loop_new(NULL,FALSE);
    m_pMW = new CMiddleWare(PLUGINS_CONF_DIR,PLUGINS_LIB_DIR);
    SetUpMW();
    SetUpCron();
    FindNewDumps(pPath);
#ifdef ENABLE_DBUS
    m_pCommLayer = new CCommLayerServerDBus();
#elif ENABLE_SOCKET
    m_pCommLayer = new CCommLayerServerSocket();
#endif
//  m_pCommLayer = new CCommLayerServerDBus();
//  m_pCommLayer = new CCommLayerServerSocket();
    m_pCommLayer->Attach(this);

    if((m_nFd = inotify_init()) == -1)
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CCrashWatcher(): Init Failed");
    }
    if((watch = inotify_add_watch(m_nFd, pPath.c_str(), IN_CREATE)) == -1){

        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::CCrashWatcher(): Add watch failed:" + pPath);
    }
    m_pGio = g_io_channel_unix_new(m_nFd);
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
    // get potencial unsaved debugdumps
    dp = opendir (pPath.c_str());
    if (dp != NULL)
    {
        while ((ep = readdir (dp)))
        {
            if(strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0)
            {
                dname = pPath + "/" + ep->d_name;
                lstat (dname.c_str(), &stats);
                if(S_ISDIR (stats.st_mode))
                {
                    dirs.push_back(dname);
                }
            }
        }
        (void) closedir (dp);
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
    if (fd >= 0) {
            FILE *f;

            /* write our pid to it */
            f = fdopen(fd, "w");
            if (f != NULL) {
                    fprintf(f, "%d\n", getpid());
                    fclose(f);
                    /* leave the fd open */
                    return;
            }
            close(fd);
    }

    /* something went wrong */
    CABRTException(EXCEP_FATAL, "CCrashWatcher::CreatePidFile(): can not open pid file");
}

void CCrashWatcher::Lock()
{
    int lfp = open(VAR_RUN_LOCK_FILE, O_RDWR|O_CREAT,0640);
	if (lfp < 0)
	{
	    throw CABRTException(EXCEP_FATAL, "CCrashWatcher::Lock(): can not open lock file");
	}
	if (lockf(lfp,F_TLOCK,0) < 0)
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
    while(1)
    {
        i = 0;
        len = read(m_nFd,buff,INOTIFY_BUFF_SIZE);
        while(i < len)
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

/* daemon loop with glib */
void CCrashWatcher::GStartWatch()
{
    g_io_add_watch (m_pGio, G_IO_IN, handle_event_cb, this);
    //enter the event loop
    g_main_run (m_pMainloop);
}


void CCrashWatcher::Daemonize()
{
    Lock();
    Debug("Daemonize...");
    // forking to background
    pid_t pid = fork();
	if (pid < 0)
    {
	    throw CABRTException(EXCEP_FATAL, "CCrashWatcher::Daemonize(): Fork error");
    }
    /* parent exits */
	if (pid > 0) _exit(0);
	/* child (daemon) continues */
    pid_t sid = setsid();
    if(sid == -1)
    {
        throw CABRTException(EXCEP_FATAL, "CCrashWatcher::Daemonize(): setsid failed");
    }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    /* we need a pid file for the child process */
    CreatePidFile();
    GStartWatch();
}

void CCrashWatcher::Run()
{
    Lock();
    CreatePidFile();
    Debug("Runnig...");
    GStartWatch();
}

vector_crash_infos_t CCrashWatcher::GetCrashInfos(const std::string &pUID)
{
    vector_crash_infos_t retval;
    Debug("Getting crash infos...");
    try
    {
        vector_strings_t UUIDs;
        UUIDs = m_pMW->GetUUIDsOfCrash(pUID);

        unsigned int ii;
        for (ii = 0; ii < UUIDs.size(); ii++)
        {
            CMiddleWare::mw_result_t res;
            map_crash_info_t info;

            res = m_pMW->GetCrashInfo(UUIDs[ii], pUID, info);
            switch(res)
            {
                case CMiddleWare::MW_OK:
                    retval.push_back(info);
                    break;
                case CMiddleWare::MW_ERROR:
                    Warning("Can not find debug dump directory for UUID: " + UUIDs[ii] + ", deleting from database");
                    Status("Can not find debug dump directory for UUID: " + UUIDs[ii] + ", deleting from database");
                    m_pMW->DeleteCrashInfo(UUIDs[ii], pUID);
                    break;
                case CMiddleWare::MW_FILE_ERROR:
                    {
                        std::string debugDumpDir;
                        Warning("Can not open file in debug dump directory for UUID: " + UUIDs[ii] + ", deleting ");
                        Status("Can not open file in debug dump directory for UUID: " + UUIDs[ii] + ", deleting ");
                        debugDumpDir = m_pMW->DeleteCrashInfo(UUIDs[ii], pUID);
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

map_crash_report_t CCrashWatcher::CreateReport(const std::string &pUUID,const std::string &pUID)
{
    map_crash_report_t crashReport;
    Debug("Creating report...");
    try
    {
        CMiddleWare::mw_result_t res;
        res = m_pMW->CreateCrashReport(pUUID,pUID,crashReport);
        switch (res)
        {
            case CMiddleWare::MW_OK:
                break;
            case CMiddleWare::MW_IN_DB_ERROR:
                Warning("Did not find crash with UUID "+pUUID+" in database.");
                Status("Did not find crash with UUID "+pUUID+" in database.");
                break;
            case CMiddleWare::MW_CORRUPTED:
            case CMiddleWare::MW_FILE_ERROR:
            default:
                {
                    std::string debugDumpDir;
                    Warning("Corrupted crash with UUID "+pUUID+", deleting.");
                    Status("Corrupted crash with UUID "+pUUID+", deleting.");
                    debugDumpDir = m_pMW->DeleteCrashInfo(pUUID, pUID);
                    m_pMW->DeleteDebugDumpDir(debugDumpDir);
                }
                break;
        }
        m_pCommLayer->AnalyzeComplete(crashReport);
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
    return crashReport;
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
        std::string home = "";
        struct passwd* pw;
        while (( pw = getpwent()) != NULL)
        {
            if (pw->pw_uid == atoi(pUID.c_str()))
            {
                home = pw->pw_dir;
            }
        }
        setpwent();
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
