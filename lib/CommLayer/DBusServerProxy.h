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
#ifndef DBUSSERVERPROXY_H_
#define DBUSSERVERPROXY_H_

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>
#include "DBusCommon.h"

class CDBusServer_adaptor
: public DBus::InterfaceAdaptor
{
public:
    CDBusServer_adaptor();
/* reveal Interface introspection when we stabilize the API */
/*
    DBus::IntrospectedInterface *const introspect() const;
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
     virtual map_crash_report_t CreateReport(const std::string &pUUID, const std::string &pDBusSender) = 0;
     virtual uint64_t CreateReport_t(const std::string &pUUID, const std::string &pDBusSender) = 0;
     virtual bool Report(map_crash_report_t pReport, const std::string &pDBusSender) = 0;
     virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pDBusSender) = 0;
     virtual map_crash_report_t GetJobResult(uint64_t pJobID, const std::string& pDBusSender) = 0;
     virtual vector_map_string_string_t GetPluginsInfo() = 0;
     virtual map_plugin_settings_t GetPluginSettings(const std::string& pName, const std::string& pDBusSender) = 0;
     virtual void SetPluginSettings(const std::string& pName, const std::string& pSender, const map_plugin_settings_t& pSettings) = 0;
     virtual void RegisterPlugin(const std::string& pName) = 0;
     virtual void UnRegisterPlugin(const std::string& pName) = 0;

public:
    /* signal emitters for this interface
     */
    /* Notify the clients (UI) about a new crash */
    void Crash(const std::string& arg1);
    /* Notify the clients that creating a report has finished */
    void AnalyzeComplete(map_crash_report_t arg1);
    void JobDone(const std::string &pDest, uint64_t job_id);
    void Error(const std::string& arg1);
    void Update(const std::string pDest, const std::string& pMessage);
    void Warning(const std::string& arg1);

private:
    /* unmarshalers (to unpack the DBus message before calling the actual interface method)
     */
    DBus::Message _GetCrashInfos_stub(const DBus::CallMessage &call);
    DBus::Message _CreateReport_stub(const DBus::CallMessage &call);
    DBus::Message _Report_stub(const DBus::CallMessage &call);
    DBus::Message _DeleteDebugDump_stub(const DBus::CallMessage &call);
    DBus::Message _GetJobResult_stub(const DBus::CallMessage &call);
    DBus::Message _GetPluginsInfo_stub(const DBus::CallMessage &call);
    DBus::Message _GetPluginSettings_stub(const DBus::CallMessage &call);
    DBus::Message _RegisterPlugin_stub(const DBus::CallMessage &call);
    DBus::Message _UnRegisterPlugin_stub(const DBus::CallMessage &call);
    DBus::Message _SetPluginSettings_stub(const DBus::CallMessage &call);
};

#endif
