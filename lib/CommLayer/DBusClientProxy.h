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
