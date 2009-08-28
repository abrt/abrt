#include <iostream>
#include "abrtlib.h"
#include "ABRTException.h"
#include "CrashWatcher.h"
#include "Daemon.h"
#include "CommLayerServerDBus.h"


void attach_dbus_dispatcher_to_glib_main_context()
{
    DBus::Glib::BusDispatcher* dispatcher;
    dispatcher = new DBus::Glib::BusDispatcher();
    dispatcher->attach(NULL);
    DBus::default_dispatcher = dispatcher;
}

DBus::Connection *CCommLayerServerDBus::init_dbus(CCommLayerServerDBus *self)
{
    self->m_pConn = new DBus::Connection(DBus::Connection::SystemBus());
    return self->m_pConn;
}

CCommLayerServerDBus::CCommLayerServerDBus()
:
    DBus::InterfaceAdaptor(CC_DBUS_IFACE),
    DBus::ObjectAdaptor(*init_dbus(this), CC_DBUS_PATH)
{
    try
    {
        m_pConn->request_name(CC_DBUS_NAME);
    }
    catch (DBus::Error err)
    {
        error_msg("error requesting DBus name %s, possible reasons: abrt run by non-root; dbus config is incorrect", CC_DBUS_NAME);
        m_init_error = 1;
        return;
    }
    register_method(CCommLayerServerDBus, GetCrashInfos, _GetCrashInfos_stub);
    register_method(CCommLayerServerDBus, CreateReport, _CreateReport_stub);
    register_method(CCommLayerServerDBus, Report, _Report_stub);
    register_method(CCommLayerServerDBus, DeleteDebugDump, _DeleteDebugDump_stub);
    register_method(CCommLayerServerDBus, GetJobResult, _GetJobResult_stub);
    register_method(CCommLayerServerDBus, GetPluginsInfo, _GetPluginsInfo_stub);
    register_method(CCommLayerServerDBus, GetPluginSettings, _GetPluginSettings_stub);
    register_method(CCommLayerServerDBus, SetPluginSettings, _SetPluginSettings_stub);
    register_method(CCommLayerServerDBus, RegisterPlugin, _RegisterPlugin_stub);
    register_method(CCommLayerServerDBus, UnRegisterPlugin, _UnRegisterPlugin_stub);
}

CCommLayerServerDBus::~CCommLayerServerDBus()
{
}


/*
 * DBus call handlers
 */

DBus::Message CCommLayerServerDBus::_GetCrashInfos_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();

    const char* sender = call.sender();
    unsigned long unix_uid = m_pConn->sender_unix_uid(sender);
    VERB1 log("got %s() call from %s, uid %lu", "GetCrashInfos", sender, unix_uid);
    vector_crash_infos_t argout1 = GetCrashInfos(to_string(unix_uid));

    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}

DBus::Message CCommLayerServerDBus::_CreateReport_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string argin1;
    ri >> argin1;

    const char* sender = call.sender();
    unsigned long unix_uid = m_pConn->sender_unix_uid(sender);
    VERB1 log("got %s('%s') call from %s, uid %lu", "CreateReport", argin1.c_str(), sender, unix_uid);
    uint64_t argout1 = CreateReport_t(argin1, to_string(unix_uid), sender);

    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}

DBus::Message CCommLayerServerDBus::_Report_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    map_crash_report_t argin1;
    ri >> argin1;

    const char* sender = call.sender();
    unsigned long unix_uid = m_pConn->sender_unix_uid(sender);
    VERB1 log("got %s(...) call from %s, uid %lu", "Report", sender, unix_uid);
    report_status_t argout1 = Report(argin1, to_string(unix_uid));

    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}

DBus::Message CCommLayerServerDBus::_DeleteDebugDump_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string argin1;
    ri >> argin1;

    const char* sender = call.sender();
    unsigned long unix_uid = m_pConn->sender_unix_uid(sender);
    VERB1 log("got %s('%s') call from %s, uid %lu", "DeleteDebugDump", argin1.c_str(), sender, unix_uid);
    bool argout1 = DeleteDebugDump(argin1, to_string(unix_uid));

    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}

DBus::Message CCommLayerServerDBus::_GetJobResult_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    uint64_t job_id;
    ri >> job_id;

    const char* sender = call.sender();
    unsigned long unix_uid = m_pConn->sender_unix_uid(sender);
    VERB1 log("got %s(%llx) call from %s, uid %lu", "GetJobResult", (long long)job_id, sender, unix_uid);
    map_crash_report_t report = GetJobResult(job_id, to_string(unix_uid));

    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << report;
    return reply;
}

DBus::Message CCommLayerServerDBus::_GetPluginsInfo_stub(const DBus::CallMessage &call)
{
    vector_map_string_string_t plugins_info = g_pPluginManager->GetPluginsInfo();

    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << plugins_info;
    return reply;
}

DBus::Message CCommLayerServerDBus::_GetPluginSettings_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string PluginName;
    ri >> PluginName;

    const char* sender = call.sender();
    unsigned long unix_uid = m_pConn->sender_unix_uid(sender);
    VERB1 log("got %s('%s') call from %s, uid %lu", "GetPluginSettings", PluginName.c_str(), sender, unix_uid);
    map_plugin_settings_t plugin_settings = g_pPluginManager->GetPluginSettings(PluginName, to_string(unix_uid));

    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << plugin_settings;
    return reply;
}

DBus::Message CCommLayerServerDBus::_SetPluginSettings_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string PluginName;
    map_plugin_settings_t plugin_settings;
    ri >> PluginName;
    ri >> plugin_settings;

    const char* sender = call.sender();
    unsigned long unix_uid = m_pConn->sender_unix_uid(sender);
    VERB1 log("got %s('%s',...) call from %s, uid %lu", "SetPluginSettings", PluginName.c_str(), sender, unix_uid);
    g_pPluginManager->SetPluginSettings(PluginName, to_string(unix_uid), plugin_settings);

    DBus::ReturnMessage reply(call);
    return reply;
}

DBus::Message CCommLayerServerDBus::_RegisterPlugin_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string PluginName;
    ri >> PluginName;

    g_pPluginManager->RegisterPlugin(PluginName);

    DBus::ReturnMessage reply(call);
    //DBus::MessageIter wi = reply.writer();
    //wi << plugin_settings;
    return reply;
}

DBus::Message CCommLayerServerDBus::_UnRegisterPlugin_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string PluginName;
    ri >> PluginName;

    g_pPluginManager->UnRegisterPlugin(PluginName);

    DBus::ReturnMessage reply(call);
    return reply;
}


/*
 * DBus signal emitters
 */

/* Notify the clients (UI) about a new crash */
void CCommLayerServerDBus::Crash(const std::string& progname, const std::string& uid)
{
    ::DBus::SignalMessage sig("Crash");
    ::DBus::MessageIter wi = sig.writer();
    wi << progname;
    wi << uid;
    emit_signal(sig);
}

/* Notify the clients that creating a report has finished */
void CCommLayerServerDBus::AnalyzeComplete(const map_crash_report_t& arg1)
{
    ::DBus::SignalMessage sig("AnalyzeComplete");
    ::DBus::MessageIter wi = sig.writer();
    wi << arg1;
    emit_signal(sig);
}

void CCommLayerServerDBus::JobDone(const std::string &pDest, uint64_t job_id)
{
    ::DBus::SignalMessage sig("JobDone");
    ::DBus::MessageIter wi = sig.writer();
    wi << pDest;
    wi << job_id;
    emit_signal(sig);
}

void CCommLayerServerDBus::JobStarted(const std::string &pDest, uint64_t job_id)
{
    ::DBus::SignalMessage sig("JobStarted");
    ::DBus::MessageIter wi = sig.writer();
    wi << pDest;
    wi << job_id;
    emit_signal(sig);
}

void CCommLayerServerDBus::Error(const std::string& arg1)
{
    ::DBus::SignalMessage sig("Error");
    ::DBus::MessageIter wi = sig.writer();
    wi << arg1;
    emit_signal(sig);
}

void CCommLayerServerDBus::Update(const std::string& pMessage, uint64_t pJobID)
{
    ::DBus::SignalMessage sig("Update");
    ::DBus::MessageIter wi = sig.writer();
    wi << pMessage;
    wi << pJobID;
    emit_signal(sig);
}

void CCommLayerServerDBus::Warning(const std::string& arg1)
{
    ::DBus::SignalMessage sig("Warning");
    ::DBus::MessageIter wi = sig.writer();
    wi << arg1;
    emit_signal(sig);
}

void CCommLayerServerDBus::Warning(const std::string& pMessage, uint64_t pJobID)
{
    ::DBus::SignalMessage sig("Warning");
    ::DBus::MessageIter wi = sig.writer();
    wi << pMessage;
    wi << pJobID;
    emit_signal(sig);
}
