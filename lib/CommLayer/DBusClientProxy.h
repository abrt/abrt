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
    
#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>
#include "DBusCommon.h"
#include <iostream>
#define ABRT_NOT_RUNNING 0
#define ABRT_RUNNING 1

namespace org {
namespace freedesktop {
namespace DBus {

class DaemonWatcher_proxy
 : public ::DBus::InterfaceProxy
{
private:
    void *m_pStateChangeHandler_cb_data;
    void (*m_pStateChangeHandler)(bool running, void* data);
public:

    DaemonWatcher_proxy()
    : ::DBus::InterfaceProxy("org.freedesktop.DBus")
    {
        m_pStateChangeHandler_cb_data = NULL;
        m_pStateChangeHandler = NULL;
        connect_signal(DaemonWatcher_proxy, NameOwnerChanged , _DaemonStateChanged);
    }

    void ConnectStateChangeHandler(void (*pStateChangeHandler)(bool running, void* data), void *cb_data)
    {
        m_pStateChangeHandler_cb_data = cb_data;
        m_pStateChangeHandler = pStateChangeHandler;
    }
private:

    /* unmarshalers (to unpack the DBus message before calling the actual signal handler)
     */
    void _DaemonStateChanged(const ::DBus::SignalMessage &sig)
    {
        ::DBus::MessageIter ri = sig.reader();
        std::string name;
        std::string old_owner;
        std::string new_owner;
        ri >> name;
        ri >> old_owner;
        ri >> new_owner;
        if(name.compare("com.redhat.abrt") == 0){ 
            if(new_owner.length() > 0)
            {
                if(m_pStateChangeHandler)
                {
                    m_pStateChangeHandler(true,m_pStateChangeHandler_cb_data);
                }
                else
                {
                    std::cout << "Daemon appeared!" << std::endl;
                }
            }
            if(new_owner.length() == 0)
            {
                if(m_pStateChangeHandler)
                {
                    m_pStateChangeHandler(false, m_pStateChangeHandler_cb_data);
                }
                else
                {
                    std::cout << "Daemon dissapeared!" << std::endl;
                }
            }
        }
    }
};

} } }

class DaemonWatcher
: public org::freedesktop::DBus::DaemonWatcher_proxy,
  public DBus::IntrospectableProxy,
  public DBus::ObjectProxy
{
public:

    DaemonWatcher(DBus::Connection &connection, const char *path, const char *name)
    : ::DBus::ObjectProxy(connection, path, name)
    {
    }
    ~DaemonWatcher()
    {
        std::cout << "~DaemonWatcher" << std::endl;
    }
};
        

class CDBusClient_proxy
 : public DBus::InterfaceProxy
{
public:

    CDBusClient_proxy()
    : DBus::InterfaceProxy(CC_DBUS_IFACE)
    {
        //# define connect_signal(interface, signal, callback)
        connect_signal(CDBusClient_proxy, Crash, _Crash_stub);
    }

public:

    /* methods exported by this interface,
     * this functions will invoke the corresponding methods on the remote objects
     */
     /*
     
     < 
      <m_sUUID;m_sUID;m_sCount;m_sExecutable;m_sPackage>
      <m_sUUID;m_sUID;m_sCount;m_sExecutable;m_sPackage>
      <m_sUUID;m_sUID;m_sCount;m_sExecutable;m_sPackage>
      ...
      >
      */
    vector_crash_infos_t GetCrashInfos()
    {
        DBus::CallMessage call;
        
        DBus::MessageIter wi = call.writer();

        call.member("GetCrashInfos");
        DBus::Message ret = invoke_method(call);
        DBus::MessageIter ri = ret.reader();

        vector_crash_infos_t argout;
        ri >> argout;
        return argout;
    }
    
    bool DeleteDebugDump(const std::string& pUUID)
    {
        DBus::CallMessage call;
        
        DBus::MessageIter wi = call.writer();

        wi << pUUID;
        call.member("DeleteDebugDump");
        DBus::Message ret = invoke_method(call);
        DBus::MessageIter ri = ret.reader();

        bool argout;
        ri >> argout;
        return argout;
    }
    
    map_crash_report_t CreateReport(const std::string& pUUID)
    {
        DBus::CallMessage call;
        
        DBus::MessageIter wi = call.writer();

        wi << pUUID;
        call.member("CreateReport");
        DBus::Message ret = invoke_method(call);
        DBus::MessageIter ri = ret.reader();

        map_crash_report_t argout;
        ri >> argout;
        return argout;
    };
    
    void Report(map_crash_report_t pReport)
    {
        DBus::CallMessage call;
        
        DBus::MessageIter wi = call.writer();

        wi << pReport;
        call.member("Report");
        DBus::Message ret = invoke_method(call);
        DBus::MessageIter ri = ret.reader();
    }
    
public:

    /* signal handlers for this interface
     */
     virtual void Crash(std::string& value) = 0;

private:

    /* unmarshalers (to unpack the DBus message before calling the actual signal handler)
     */
    void _Crash_stub(const ::DBus::SignalMessage &sig)
    {
        DBus::MessageIter ri = sig.reader();

        std::string value; ri >> value;
        Crash(value);
    }
};
