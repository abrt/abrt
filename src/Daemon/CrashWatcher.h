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
    
#ifndef CRASHWATCHER_H_
#define CRASHWATCHER_H_

#include <string>
#include <sys/inotify.h>
#include <sys/inotify.h>
#include <glib.h>
#include "DBusManager.h"
#include "MiddleWare.h"

// 1024 simultaneous actions
#define INOTIFY_BUFF_SIZE ((sizeof(struct inotify_event)+FILENAME_MAX)*1024)


class CCrashWatcher
{
    private:
        static gboolean handle_event_cb(GIOChannel *gio, GIOCondition condition, gpointer data);
        void RegisterSignals();
        void StartWatch();
        void GStartWatch();
        void Lock();
        
        CDBusManager m_nDbus_manager;
        int m_nFd;
        GIOChannel* m_nGio;
        GMainLoop *m_nMainloop;
        std::string m_sTarget;
	public:
        CCrashWatcher(const std::string& pPath);
        //CCrashWatcher();
		~CCrashWatcher();
        //run as daemon
        void Daemonize();
        //don't go background - for debug
        void Run();
};

#endif /*CRASHWATCHER_H_*/
