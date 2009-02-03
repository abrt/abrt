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
    
#ifndef DBUS_H_
#define DBUS_H_

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <string>

#define CC_DBUS_NAME "com.redhat.CrashCatcher"
#define CC_DBUS_PATH "/com/redhat/CrashCatcher"
#define CC_DBUS_IFACE "com.redhat.CrashCatcher"
#define DBUS_BUS DBUS_BUS_SYSTEM
#define CC_DBUS_PATH_NOTIFIER "/com/redhat/CrashCatcher/Crash"

class CDBusManager
{
    private:
        DBusGConnection *m_nBus;
        DBusGProxy *m_nBus_proxy;
        DBusGProxy *m_nCCBus_proxy;
        
	public:
        CDBusManager();
        ~CDBusManager();
        bool SendMessage(const std::string& pMessage, const std::string& pMessParam);
        bool GSendMessage(const std::string& pMessage, const std::string& pMessParam);
        void RegisterService();
        void ConnectToService();
        void ConnectToDaemon();
        void LoopSend();
        void Unregister();
        void RegisterToMessage(const std::string& pMessage, GCallback handler, void * data, GClosureNotify free_data_func);
        /** TODO
        //tries to reconnect after daemon failure
        void Reconnect();
        */
};

#endif /*DBUS_H_*/
