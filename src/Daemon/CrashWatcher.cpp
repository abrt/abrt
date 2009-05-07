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
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <csignal>
#include <sstream>
#include <dirent.h>
#include <cstring>
#include "CommLayerInner.h"
#include "ABRTException.h"

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
    if (err != G_IO_ERROR_NONE) {
            g_warning ("Error reading inotify fd: %d\n", err);
            return FALSE;
    }
    /* reconstruct each event and send message to the dbus */
    while (i < len) {
        const char *name = NULL;
        struct inotify_event *event;

        event = (struct inotify_event *) &buf[i];
        if (event->len)
                name = &buf[i] + sizeof (struct inotify_event);
        i += sizeof (struct inotify_event) + event->len;
#ifdef DEBUG
        std::cout << "Created file: " << name << std::endl;
#endif /*DEBUG*/

        /* we want to ignore the lock files */
        if(event->mask & IN_ISDIR)
        {
            CCrashWatcher *cc = (CCrashWatcher*)daemon;
#ifdef DEBUG
            std::cerr << cc->GetDirSize(DEBUG_DUMPS_DIR)/(1024*1024.0) << std::endl;
            std::cerr << cc->m_pSettings->GetMaxCrashReportsSize() << std::endl;
#endif /*DEBUG*/
            if(cc->GetDirSize(DEBUG_DUMPS_DIR)/(1024*1024.0) < cc->m_pSettings->GetMaxCrashReportsSize()){
                //std::string sName = name;
                map_crash_info_t crashinfo;
                try
                {
                    if(cc->m_pMW->SaveDebugDump(std::string(DEBUG_DUMPS_DIR) + "/" + name, crashinfo))
                    {
                        cc->m_pMW->Report(crashinfo[CD_MWDDD][CD_CONTENT]);
                        /* send message to dbus */
                        cc->m_pCommLayer->Crash(crashinfo[CD_PACKAGE][CD_CONTENT]);
                    }
                }
                catch (CABRTException& e)
                {
                    std::cerr << e.what() << std::endl;
                    if (e.type() == EXCEP_ERROR)
                    {
                        return -1;
                    }
                }
            }
            else
            {
#ifdef DEBUG
                std::cout << "DebugDumps size has exceeded the limit, deleting the last dump." << name << std::endl;
#endif /*DEBUG*/
                cc->m_pMW->DeleteDebugDumpDir(std::string(DEBUG_DUMPS_DIR) + "/" + name);
            }
        }
#ifdef DEBUG
        else
        {
            std::cerr << "Some file created, ignoring.." << std::endl;
        }
#endif /*DEBUG*/
    }
    delete[] buf;
    return TRUE;
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
    CSettings::set_pair_strings_t reporters = m_pSettings->GetReporters();
    CSettings::set_pair_strings_t::iterator it_r;
    for (it_r = reporters.begin(); it_r != reporters.end(); it_r++)
    {
        m_pMW->AddReporter((*it_r).first, (*it_r).second);
    }
    CSettings::map_analyzer_reporters_t analyzer_reporters = m_pSettings->GetAnalyzerReporters();
    CSettings::map_analyzer_reporters_t::iterator it_pr;
    for (it_pr = analyzer_reporters.begin(); it_pr != analyzer_reporters.end(); it_pr++)
    {
        CSettings::set_pair_strings_t::iterator it_r;
        for (it_r = it_pr->second.begin(); it_r != it_pr->second.end(); it_r++)
        {
            m_pMW->AddAnalyzerReporter(it_pr->first, (*it_r).first, (*it_r).second);
        }
    }
    CSettings::map_analyzer_actions_t analyser_actions = m_pSettings->GetAnalyzerActions();
    CSettings::map_analyzer_actions_t::iterator it_pa;
    for (it_pa = analyser_actions.begin(); it_pa != analyser_actions.end(); it_pa++)
    {
        CSettings::set_pair_strings_t::iterator it_a;
        for (it_a = it_pa->second.begin(); it_a != it_pa->second.end(); it_a++)
        {
            m_pMW->AddAnalyzerAction(it_pa->first, (*it_a).first, (*it_a).second);
        }
    }
}

void CCrashWatcher::Status(const std::string& pMessage)
{
    std::cout << "Update: " << pMessage << std::endl;
}

void CCrashWatcher::Warning(const std::string& pMessage)
{
    std::cerr << "Warning: " << pMessage << std::endl;
}

void CCrashWatcher::Debug(const std::string& pMessage)
{
    //some logic to add logging levels?
    std::cout << "Debug: " << pMessage << std::endl;
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
        throw std::string("Init Failed");
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
    FindNewDumps(pPath);
#ifdef HAVE_DBUS
    m_pCommLayer = new CCommLayerServerDBus();
#elif HAVE_SOCKET
    m_pCommLayer = new CCommLayerServerSocket();
#endif
    m_pCommLayer = new CCommLayerServerDBus();
    m_pCommLayer->Attach(this);

    if((m_nFd = inotify_init()) == -1){
        throw std::string("Init Failed");
        //std::cerr << "Init Failed" << std::endl;
        exit(-1);
    }
    if((watch = inotify_add_watch(m_nFd, pPath.c_str(), IN_CREATE)) == -1){

        throw std::string("Add watch failed:") + pPath.c_str();
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
}
void CCrashWatcher::FindNewDumps(const std::string& pPath)
{
    std::cerr << "Scanning for unsaved entries" << std::endl;
    struct dirent *ep;
    struct stat stats;
    DIR *dp;
    std::vector<std::string> dirs;
    std::string dname;
    // get potencial unsaved debugdumps
    dp = opendir (pPath.c_str());
    if (dp != NULL)
    {
        while ((ep = readdir (dp))){
            if(strcmp(ep->d_name, ".") != 0 && strcmp(ep->d_name, "..") != 0){
                dname = pPath + "/" + ep->d_name;
                std::cerr << dname << std::endl;
                lstat (dname.c_str(), &stats);
                if(S_ISDIR (stats.st_mode)){
                    std::cerr << ep->d_name << std::endl;
                    dirs.push_back(dname);
                }
            }
        }
        (void) closedir (dp);
    }
    else
        perror ("Couldn't open the directory");

    for (std::vector<std::string>::iterator itt = dirs.begin(); itt != dirs.end(); ++itt){
        map_crash_info_t crashinfo;
        std::cerr << "Saving debugdeump: " << *itt << std::endl;
        try
        {
            if(m_pMW->SaveDebugDump(*itt, crashinfo))
            {
                std::cerr << "Saved new entry: " << *itt << std::endl;
                m_pMW->Report(*itt);
            }
        }
        catch (CABRTException& e)
        {
            std::cerr << e.what() << std::endl;
            if (e.type() == EXCEP_ERROR)
            {
                exit(-1);
            }
        }
    }
}

void CCrashWatcher::Lock()
{
    int lfp = open("abrt.lock",O_RDWR|O_CREAT,0640);
	if (lfp < 0)
        throw std::string("CCrashWatcher.cpp:can not open lock file");
	if (lockf(lfp,F_TLOCK,0) < 0)
        throw std::string("CCrashWatcher.cpp:Lock:cannot create lock on lockfile");
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
    while(1){
        i = 0;
        len = read(m_nFd,buff,INOTIFY_BUFF_SIZE);
        while(i < len){
            pevent = (struct inotify_event *)&buff[i];
            if (pevent->len)
                std::strcpy(action, pevent->name);
            else
                std::strcpy(action, m_sTarget.c_str());
            i += sizeof(struct inotify_event) + pevent->len;
#ifdef DEBUG
            std::cout << "Created file: " << action << std::endl;
#endif /*DEBUG*/
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
#ifdef DEBUG
    std::cout << "Daemonize" << std::endl;
#endif
    // forking to background
    pid_t pid = fork();
	if (pid < 0)
    {
        throw "CCrashWatcher.cpp:Daemonize:Fork error";
    }
    /* parent exits */
	if (pid > 0) _exit(0);
	/* child (daemon) continues */
    pid_t sid = setsid();
    if(sid == -1){
        throw "CCrashWatcher.cpp:Daemonize:setsid failed";
    }
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    //Lock();
    GStartWatch();
}

void CCrashWatcher::Run()
{
#ifdef DEBUG
    std::cout << "Run" << std::endl;
#endif
    //Lock();
    GStartWatch();
}

vector_crash_infos_t CCrashWatcher::GetCrashInfos(const std::string &pUID)
{
    vector_crash_infos_t retval;
    std::cerr << "CCommLayerServerDBus::GetCrashInfos" << std::endl;
    try
    {
        retval = m_pMW->GetCrashInfos(pUID);
    }
    catch (CABRTException& e)
    {
        std::cerr << e.what() << std::endl;
        m_pCommLayer->Error(e.what());
        if (e.type() == EXCEP_ERROR)
        {
            exit(-1);
        }
    }
    //Notify("Sent crash info");
	return retval;
}

map_crash_report_t CCrashWatcher::CreateReport(const std::string &pUUID,const std::string &pUID)
{
    map_crash_report_t crashReport;
    std::cerr << "Creating report" << std::endl;
    try
    {
        m_pMW->CreateCrashReport(pUUID,pUID,crashReport);
        m_pCommLayer->AnalyzeComplete(crashReport);
    }
    catch (CABRTException& e)
    {
        std::cerr << e.what() << std::endl;
        m_pCommLayer->Error(e.what());
        if (e.type() == EXCEP_ERROR)
        {
            exit(-1);
        }
    }
    return crashReport;
}

bool CCrashWatcher::Report(map_crash_report_t pReport)
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
        m_pMW->Report(pReport);
    }
    catch (CABRTException& e)
    {
        std::cerr << e.what() << std::endl;
        m_pCommLayer->Error(e.what());
        if (e.type() == EXCEP_ERROR)
        {
            exit(-1);
        }
        return false;
    }
    return true;
}

bool CCrashWatcher::DeleteDebugDump(const std::string& pUUID, const std::string& pUID)
{
    try
    {
        m_pMW->DeleteCrashInfo(pUUID,pUID, true);
    }
    catch (CABRTException& e)
    {
        std::cerr << e.what() << std::endl;
        m_pCommLayer->Error(e.what());
        if (e.type() == EXCEP_ERROR)
        {
            return -1;
        }
        return false;
    }
    return true;
}
