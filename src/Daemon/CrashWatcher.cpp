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

void terminate(int signal)
{
    exit(0);
}
/* just a helper function */
template< class T >
std::string
to_string( T x )
{
    std::ostringstream o;
    o << x;
    return o.str();
}

gboolean CCrashWatcher::handle_event_cb(GIOChannel *gio, GIOCondition condition, gpointer daemon){
    GIOError err;
    char buf[INOTIFY_BUFF_SIZE];
    gsize len;
    int i = 0;
    err = g_io_channel_read (gio, buf, INOTIFY_BUFF_SIZE, &len);
    if (err != G_IO_ERROR_NONE) {
            g_warning ("Error reading inotify fd: %d\n", err);
            return FALSE;
    }
    /* reconstruct each event and send message to the dbus */
    while (i < len) {
        const char *name;
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
            crash_info_t crashinfo;
            if(cc->m_pMW->SaveDebugDump(std::string(DEBUG_DUMPS_DIR) + "/" + name, crashinfo))
            {
                /* send message to dbus */
                cc->Crash(crashinfo.m_sPackage);
            }
        }
#ifdef DEBUG
        else
        {
            std::cerr << "Some file created, ignoring.." << std::endl;
        }
#endif /*DEBUG*/
    }
    return TRUE;
}

CCrashWatcher::CCrashWatcher(const std::string& pPath,DBus::Connection &connection)
: DBus::ObjectAdaptor(connection, CC_DBUS_PATH)
{
    m_pConn = &connection;
    int watch = 0;
    m_sTarget = pPath;
    // middleware object
    m_pMW = new CMiddleWare(PLUGINS_CONF_DIR,PLUGINS_LIB_DIR, std::string(CONF_DIR) + "/CrashCatcher.conf");
    m_nMainloop = g_main_loop_new(NULL,FALSE);
    connection.request_name(CC_DBUS_NAME);
    if((m_nFd = inotify_init()) == -1){
        throw std::string("Init Failed");
        //std::cerr << "Init Failed" << std::endl;
        exit(-1);
    }
    if((watch = inotify_add_watch(m_nFd, pPath.c_str(), IN_CREATE)) == -1){
        throw std::string("Add watch failed");
    }
    m_nGio = g_io_channel_unix_new(m_nFd);
}

CCrashWatcher::~CCrashWatcher()
{
     //delete dispatcher, connection, etc..
}

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

dbus_vector_map_crash_infos_t CCrashWatcher::GetCrashInfosMap(const std::string &pUID)
{
    dbus_vector_map_crash_infos_t retval;
    vector_crash_infos_t crash_info;
    std::cerr << pUID << std::endl;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pUID.c_str());
    std::cerr << "Run by user with uid: " << unix_uid << std::endl;
    crash_info = m_pMW->GetCrashInfos(to_string(unix_uid));
    for (vector_crash_infos_t::iterator it = crash_info.begin(); it!=crash_info.end(); ++it) {
        /* push the map with DB row into retval */
        map_crash_t tmp = it->GetMap(); 
        std::cout << "Time:" << tmp["Time"] << std::endl;
        retval.push_back(it->GetMap());
    }
	return retval;
}

void CCrashWatcher::Lock()
{
    int lfp = open("crashcatcher.lock",O_RDWR|O_CREAT,0640);
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
    char *buff = new char[INOTIFY_BUFF_SIZE];
    int len = 0;
    int i = 0;
    char action[FILENAME_MAX];
    struct inotify_event *pevent;
    g_io_add_watch (m_nGio, G_IO_IN, handle_event_cb, this);
    //enter the event loop
    g_main_run (m_nMainloop);
    delete[] buff;
}

void CCrashWatcher::RegisterSignals()
{
    signal(SIGTERM, terminate);
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
    Lock();
    GStartWatch();
}

void CCrashWatcher::Run()
{
#ifdef DEBUG
    std::cout << "Run" << std::endl;
#endif
    Lock();
    GStartWatch();
}

