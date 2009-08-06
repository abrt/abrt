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
#include <map>
#include <iostream>
#include <stdlib.h>

class CDBusServer_adaptor
: public DBus::InterfaceAdaptor
{
public:

    CDBusServer_adaptor()
    : DBus::InterfaceAdaptor(CC_DBUS_IFACE)
    {
        register_method(CDBusServer_adaptor, GetCrashInfos, _GetCrashInfos_stub);
        register_method(CDBusServer_adaptor, CreateReport, _CreateReport_stub);
        register_method(CDBusServer_adaptor, Report, _Report_stub);
        register_method(CDBusServer_adaptor, DeleteDebugDump, _DeleteDebugDump_stub);
        register_method(CDBusServer_adaptor, GetJobResult, _GetJobResult_stub);
    }
/* reveal Interface introspection when we stabilize the API */
/*
    DBus::IntrospectedInterface *const introspect() const
    {
        static DBus::IntrospectedArgument GetCrashInfos_args[] =
        {
            //{ "uid", "i", true},
            { "info", "a{ss}", false },
            { 0, 0, 0 }
        };
        static DBus::IntrospectedArgument Crash_args[] =
        {
            { "package", "s", false },
            { 0, 0, 0 }
        };
        static DBus::IntrospectedMethod CDBusServer_adaptor_methods[] =
        {
            { "GetCrashInfos", GetCrashInfos_args },
            { 0, 0 },
            { "GetCrashInfosMap", GetCrashInfos_args },
            { 0, 0 }
        };
        static DBus::IntrospectedMethod CDBusServer_adaptor_signals[] =
        {
            { "Crash", Crash_args },
            { 0, 0 }
        };
        static DBus::IntrospectedProperty CDBusServer_adaptor_properties[] =
        {
            { 0, 0, 0, 0 }
        };
        static DBus::IntrospectedInterface CDBusServer_adaptor_interface =
        {
            "com.redhat.abrt",
            CDBusServer_adaptor_methods,
            CDBusServer_adaptor_signals,
            CDBusServer_adaptor_properties
        };
        return &CDBusServer_adaptor_interface;
    }
*/
public:

    /* properties exposed by this interface, use
     * property() and property(value) to get and set a particular property
     */

public:

    /* methods exported by this interface,
     * you will have to implement them in your ObjectAdaptor
     */
     virtual vector_crash_infos_t GetCrashInfos(const std::string &pDBusSender) = 0;
     virtual map_crash_report_t CreateReport(const std::string &pUUID,const std::string &pDBusSender) = 0;
     virtual uint64_t CreateReport_t(const std::string &pUUID,const std::string &pDBusSender) = 0;
     virtual bool Report(map_crash_report_t pReport, const std::string &pDBusSender) = 0;
     virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pDBusSender) = 0;
     virtual map_crash_report_t GetJobResult(uint64_t pJobID, const std::string& pDBusSender) = 0;


public:
    /* signal emitters for this interface
     */
    /* Notify the clients (UI) about a new crash */
    void Crash(const std::string& arg1)
    {
        ::DBus::SignalMessage sig("Crash");
        ::DBus::MessageIter wi = sig.writer();
        wi << arg1;
        emit_signal(sig);
    }
    /* Notify the clients that creating a report has finished */
    void AnalyzeComplete(map_crash_report_t arg1)
    {
        ::DBus::SignalMessage sig("AnalyzeComplete");
        ::DBus::MessageIter wi = sig.writer();
        wi << arg1;
        emit_signal(sig);
    }

    void JobDone(const std::string &pDest, uint64_t job_id)
    {
        ::DBus::SignalMessage sig("JobDone");
        ::DBus::MessageIter wi = sig.writer();
        wi << pDest;
        wi << job_id;
        emit_signal(sig);
    }

    void Error(const std::string& arg1)
    {
        ::DBus::SignalMessage sig("Error");
        ::DBus::MessageIter wi = sig.writer();
        wi << arg1;
        emit_signal(sig);
    }

    void Update(const std::string pDest, const std::string& pMessage)
    {
        ::DBus::SignalMessage sig("Update");
        ::DBus::MessageIter wi = sig.writer();
        wi << pDest;
        wi << pMessage;
        emit_signal(sig);
    }

    void Warning(const std::string& arg1)
    {
        ::DBus::SignalMessage sig("Warning");
        ::DBus::MessageIter wi = sig.writer();
        wi << arg1;
        emit_signal(sig);
    }

private:

    /* unmarshalers (to unpack the DBus message before calling the actual interface method)
     */
    DBus::Message _GetCrashInfos_stub(const DBus::CallMessage &call)
    {
        DBus::MessageIter ri = call.reader();
        //FIXME: @@@REMOVE!!
        vector_crash_infos_t argout1 = GetCrashInfos(call.sender());
        DBus::ReturnMessage reply(call);
        DBus::MessageIter wi = reply.writer();
        wi << argout1;
        return reply;
    }

    DBus::Message _CreateReport_stub(const DBus::CallMessage &call)
    {
        DBus::MessageIter ri = call.reader();

        std::string argin1; ri >> argin1;
        uint64_t argout1 = CreateReport_t(argin1,call.sender());
        if(sizeof (uint64_t) != 8) abort ();
        //map_crash_report_t argout1 = CreateReport(argin1,call.sender());
        DBus::ReturnMessage reply(call);
        DBus::MessageIter wi = reply.writer();
        wi << argout1;
        return reply;
    }

    DBus::Message _Report_stub(const DBus::CallMessage &call)
    {
        DBus::MessageIter ri = call.reader();

        map_crash_report_t argin1; ri >> argin1;
        bool argout1 = Report(argin1, call.sender());
        DBus::ReturnMessage reply(call);
        DBus::MessageIter wi = reply.writer();
        wi << argout1;
        return reply;
    }

    DBus::Message _DeleteDebugDump_stub(const DBus::CallMessage &call)
    {
        DBus::MessageIter ri = call.reader();

        std::string argin1; ri >> argin1;
        bool argout1 = DeleteDebugDump(argin1,call.sender());
        DBus::ReturnMessage reply(call);
        DBus::MessageIter wi = reply.writer();
        wi << argout1;
        return reply;
    }

    DBus::Message _GetJobResult_stub(const DBus::CallMessage &call)
    {
        DBus::MessageIter ri = call.reader();
        uint64_t job_id;
        ri >> job_id;
        map_crash_report_t report = GetJobResult(job_id,call.sender());
        DBus::ReturnMessage reply(call);
        DBus::MessageIter wi = reply.writer();
        wi << report;
        return reply;
    }
};

