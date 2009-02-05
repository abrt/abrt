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

void terminate(int signal)
{
    exit(0);
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
    /* reconstruct each event and send to the user's callback */
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
        /* send message to dbus */
        CCrashWatcher *cc = (CCrashWatcher*)daemon;
        cc->m_nDbus_manager.SendMessage("Crash", name);
    }
    return TRUE;
}

CCrashWatcher::CCrashWatcher(const std::string& pPath)
{
    int watch = 0;
    m_sTarget = pPath;
    m_nMainloop = g_main_loop_new(NULL,FALSE);
    /* register on dbus */
    m_nDbus_manager.RegisterService();
    if((m_nFd = inotify_init()) == -1){
        throw std::string("Init Failed");
        //std::cerr << "Init Failed" << std::endl;
        exit(-1);
    }
    if((watch = inotify_add_watch(m_nFd, pPath.c_str(), IN_CREATE)) == -1){
        throw std::string("Add watch failed");
        //std::cerr << "Add watch failed: " << pPath << std::endl;
        exit(-1);
    }
    m_nGio = g_io_channel_unix_new(m_nFd);
}

CCrashWatcher::~CCrashWatcher()
{
}

void CCrashWatcher::Lock()
{
    int lfp = open("crashcatcher.lock",O_RDWR|O_CREAT,0640);
	if (lfp < 0) 
        throw "CCrashWatcher.cpp:can not open lock file";
	if (lockf(lfp,F_TLOCK,0) < 0)
        throw "CCrashWatcher.cpp:Lock:cannot create lock on lockfile";
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

