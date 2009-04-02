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
    //char *buf = malloc(INOTIFY_BUFF_SIZE;
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
            std::string sName = name;
            CCrashWatcher *cc = (CCrashWatcher*)daemon;
            map_crash_info_t crashinfo;
            try
            {
                if(cc->m_pMW->SaveDebugDump(std::string(DEBUG_DUMPS_DIR) + "/" + name, crashinfo))
                {
                    /* send message to dbus */
                    cc->m_pCommLayer->Crash(crashinfo[item_crash_into_t_str[CI_PACKAGE]][CD_CONTENT]);
                }
            }
            catch(std::string err)
            {
                std::cerr << err << std::endl;
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
/*
CCrashWatcher::CCrashWatcher(const std::string& pPath,DBus::Connection &connection)
: DBus::ObjectAdaptor(connection, CC_DBUS_PATH)
{
    m_pConn = &connection;
    int watch = 0;
    m_sTarget = pPath;
    // middleware object
    m_pMW = new CMiddleWare(PLUGINS_CONF_DIR,PLUGINS_LIB_DIR, std::string(CONF_DIR) + "/abrt.conf");
    FindNewDumps(pPath);
    m_pMainloop = g_main_loop_new(NULL,FALSE);
    connection.request_name(CC_DBUS_NAME);
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
*/

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
    CSettings::map_analyzer_reporters_t reporters = m_pSettings->GetReporters();
    CSettings::map_analyzer_reporters_t::iterator it_pr;
    for (it_pr = reporters.begin(); it_pr != reporters.end(); it_pr++)
    {
        CSettings::set_strings_t::iterator it_r;
        for (it_r = it_pr->second.begin(); it_r != it_pr->second.end(); it_r++)
        {
            m_pMW->AddAnalyzerReporter(it_pr->first, *it_r);
        }
    }
    CSettings::map_analyzer_actions_t actions = m_pSettings->GetActions();
    CSettings::map_analyzer_actions_t::iterator it_pa;
    for (it_pa = actions.begin(); it_pa != actions.end(); it_pa++)
    {
        CSettings::set_actions_t::iterator it_a;
        for (it_a = it_pa->second.begin(); it_a != it_pa->second.end(); it_a++)
        {
            m_pMW->AddAnalyzerAction(it_pa->first, (*it_a).first, (*it_a).second);
        }
    }
}

CCrashWatcher::CCrashWatcher(const std::string& pPath)
{
    int watch = 0;
    m_sTarget = pPath;
    // middleware object
    m_pSettings = new CSettings();
    m_pSettings->LoadSettings(std::string(CONF_DIR) + "/abrt.conf");
    m_pMW = new CMiddleWare(PLUGINS_CONF_DIR,PLUGINS_LIB_DIR);
    SetUpMW();
    FindNewDumps(pPath);
    m_pMainloop = g_main_loop_new(NULL,FALSE);
#ifdef HAVE_DBUS
    m_pCommLayer = new CCommLayerServerDBus(m_pMW);
#elif HAVE_SOCKET
    m_pCommLayer = new CCommLayerServerSocket(m_pMW);
#endif
    m_pCommLayer = new CCommLayerServerDBus(m_pMW);
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
            }
        }
        catch(std::string err)
        {
            std::cerr << err << std::endl;
        }
    }
}
/*
dbus_vector_crash_infos_t CCrashWatcher::GetCrashInfos(const std::string &pUID)
{
    dbus_vector_crash_infos_t retval;
    vector_crash_infos_t crash_info;
    m_pMW->GetCrashInfos("501");
    for (vector_crash_infos_t::iterator it = crash_info.begin(); it!=crash_info.end(); ++it) {
        std::cerr << it->m_sExecutable << std::endl;
    }
	return retval;
}

dbus_vector_map_crash_infos_t CCrashWatcher::GetCrashInfosMap(const std::string &pDBusSender)
{
    dbus_vector_map_crash_infos_t retval;
    vector_crash_infos_t crash_info;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pDBusSender.c_str());
    try
    {
        crash_info = m_pMW->GetCrashInfos(to_string(unix_uid));
    }
    catch(std::string err)
    {
        std::cerr << err << std::endl;
    }
    for (vector_crash_infos_t::iterator it = crash_info.begin(); it!=crash_info.end(); ++it) {
        retval.push_back(it->GetMap());
    }
	return retval;
}

dbus_map_report_info_t CCrashWatcher::CreateReport(const std::string &pUUID,const std::string &pDBusSender)
{
    dbus_map_report_info_t retval;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pDBusSender.c_str());
    //std::cerr << pUUID << ":" << unix_uid << std::endl;
    crash_report_t crashReport;
    std::cerr << "Creating report" << std::endl;
    try
    {
        m_pMW->CreateReport(pUUID,to_string(unix_uid), crashReport);
        retval = crashReport.GetMap();
        //send out the message about completed analyze
        AnalyzeComplete(retval);
    }
    catch(std::string err)
    {
        Error(err);
        std::cerr << err << std::endl;
    }
    return retval;
}

bool CCrashWatcher::Report(dbus_map_report_info_t pReport)
{
    crash_report_t crashReport;
    //#define FIELD(X) crashReport.m_s##X = pReport[#X];
    //crashReport.m_sUUID = pReport["UUID"];
    //ALL_CRASH_REPORT_FIELDS;
    //#undef FIELD
    //for (dbus_map_report_info_t::iterator it = pReport.begin(); it!=pReport.end(); ++it) {
    //     std::cerr << it->second << std::endl;
    //}
    crashReport.SetFromMap(pReport);
    try
    {
        m_pMW->Report(crashReport);
    }
    catch(std::string err)
    {
        std::cerr << err << std::endl;
        return false;
    }
    return true;
}

bool CCrashWatcher::DeleteDebugDump(const std::string& pUUID, const std::string& pDBusSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pDBusSender.c_str());
    try
    {
        //std::cerr << "DeleteDebugDump(" << pUUID << "," << unix_uid << ")" << std::endl;
        m_pMW->DeleteCrashInfo(pUUID,to_string(unix_uid), true);
    }
    catch(std::string err)
    {
        std::cerr << err << std::endl;
        return false;
    }
    return true;
}
*/
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

