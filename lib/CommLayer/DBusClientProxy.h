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
#ifndef DBUSCLIENTPROXY_H_
#define DBUSCLIENTPROXY_H_

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>
#include "DBusCommon.h"

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
    DaemonWatcher_proxy();
    void ConnectStateChangeHandler(void (*pStateChangeHandler)(bool running, void* data), void *cb_data);

private:
    /* unmarshalers (to unpack the DBus message before calling the actual signal handler)
     */
    void _DaemonStateChanged(const ::DBus::SignalMessage &sig);
};

} } }


class DaemonWatcher
: public org::freedesktop::DBus::DaemonWatcher_proxy,
  public DBus::IntrospectableProxy,
  public DBus::ObjectProxy
{
public:

    DaemonWatcher(DBus::Connection &connection, const char *path, const char *name);
    ~DaemonWatcher();
};


class CDBusClient_proxy
: public DBus::InterfaceProxy
{
private:
    bool m_bJobDone;
    uint64_t m_iPendingJobID;
    GMainLoop *gloop;
    std::string m_sConnName;

public:
    CDBusClient_proxy();
    CDBusClient_proxy(::DBus::Connection &pConnection);

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
    vector_crash_infos_t GetCrashInfos();
    bool DeleteDebugDump(const std::string& pUUID);
    map_crash_report_t CreateReport(const std::string& pUUID);
    void Report(map_crash_report_t pReport);
    map_crash_report_t GetJobResult(uint64_t pJobID);

public:
    /* signal handlers for this interface
     */
    virtual void Crash(std::string& value);

private:
    /* unmarshalers (to unpack the DBus message before calling the actual signal handler)
     */
    void _Crash_stub(const ::DBus::SignalMessage &sig);
    void _JobDone_stub(const ::DBus::SignalMessage &sig);
};

#endif
