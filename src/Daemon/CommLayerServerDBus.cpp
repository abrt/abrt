#include "abrtlib.h"
#include "CommLayerServerDBus.h"
#include <iostream>
#include "ABRTException.h"

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
        throw CABRTException(EXCEP_FATAL, std::string(__func__) +
                             "\nPlease check if:\n"
                             + " * abrt is being run with root permissions\n"
                             + " * you have reloaded the dbus\n"+
                             + "Original exception was:\n " + err.what());
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

/* unmarshaler (non-virtual private function) */
DBus::Message CCommLayerServerDBus::_GetCrashInfos_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    //FIXME: @@@REMOVE!!
    vector_crash_infos_t argout1 = GetCrashInfos(call.sender());
    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}
/* handler (public function) */
vector_crash_infos_t CCommLayerServerDBus::GetCrashInfos(const std::string &pSender)
{
    vector_crash_infos_t crashInfos;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    crashInfos = m_pObserver->GetCrashInfos(to_string(unix_uid));
    return crashInfos;
}

DBus::Message CCommLayerServerDBus::_CreateReport_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();

    std::string argin1;
    ri >> argin1;
    uint64_t argout1 = CreateReport_t(argin1, call.sender());
    if (sizeof (uint64_t) != 8) abort ();
    //map_crash_report_t argout1 = CreateReport(argin1,call.sender());
    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}
uint64_t CCommLayerServerDBus::CreateReport_t(const std::string &pUUID,const std::string &pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    map_crash_report_t crashReport;
    uint64_t job_id = m_pObserver->CreateReport_t(pUUID, to_string(unix_uid), pSender);
    return job_id;
}

DBus::Message CCommLayerServerDBus::_Report_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();

    map_crash_report_t argin1;
    ri >> argin1;
    report_status_t argout1 = Report(argin1, call.sender());
    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}
report_status_t CCommLayerServerDBus::Report(const map_crash_report_t& pReport, const std::string &pSender)
{
    report_status_t rs;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    rs = m_pObserver->Report(pReport, to_string(unix_uid));
    return rs;
}

DBus::Message CCommLayerServerDBus::_DeleteDebugDump_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();

    std::string argin1;
    ri >> argin1;
    bool argout1 = DeleteDebugDump(argin1, call.sender());
    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << argout1;
    return reply;
}
bool CCommLayerServerDBus::DeleteDebugDump(const std::string& pUUID, const std::string& pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    m_pObserver->DeleteDebugDump(pUUID,to_string(unix_uid));
    return true;
}

DBus::Message CCommLayerServerDBus::_GetJobResult_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    uint64_t job_id;
    ri >> job_id;
    map_crash_report_t report = GetJobResult(job_id, call.sender());
    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << report;
    return reply;
}
map_crash_report_t CCommLayerServerDBus::GetJobResult(uint64_t pJobID, const std::string& pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    map_crash_report_t crashReport;
    crashReport = m_pObserver->GetJobResult(pJobID,to_string(unix_uid));
    return crashReport;
}

DBus::Message CCommLayerServerDBus::_GetPluginsInfo_stub(const DBus::CallMessage &call)
{
    vector_map_string_string_t plugins_info;
    plugins_info = GetPluginsInfo();
    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << plugins_info;
    return reply;
}
vector_map_string_string_t CCommLayerServerDBus::GetPluginsInfo()
{
    //FIXME: simplify?
    vector_map_string_string_t plugins_info;
    plugins_info = m_pObserver->GetPluginsInfo();
    return plugins_info;
}

DBus::Message CCommLayerServerDBus::_GetPluginSettings_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string PluginName;
    std::string uid;
    ri >> PluginName;
    map_plugin_settings_t plugin_settings;
    plugin_settings = GetPluginSettings(PluginName, call.sender());
    DBus::ReturnMessage reply(call);
    DBus::MessageIter wi = reply.writer();
    wi << plugin_settings;
    return reply;
}
map_plugin_settings_t CCommLayerServerDBus::GetPluginSettings(const std::string& pName, const std::string& pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    return m_pObserver->GetPluginSettings(pName, to_string(unix_uid));
}

DBus::Message CCommLayerServerDBus::_SetPluginSettings_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string PluginName;
    map_plugin_settings_t plugin_settings;
    ri >> PluginName;
    ri >> plugin_settings;
    SetPluginSettings(PluginName, call.sender(), plugin_settings);
    DBus::ReturnMessage reply(call);
    return reply;
}
void CCommLayerServerDBus::SetPluginSettings(const std::string& pName, const std::string& pSender, const map_plugin_settings_t& pSettings)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    return m_pObserver->SetPluginSettings(pName, to_string(unix_uid), pSettings);
}

DBus::Message CCommLayerServerDBus::_RegisterPlugin_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string PluginName;
    ri >> PluginName;
    RegisterPlugin(PluginName);
    DBus::ReturnMessage reply(call);
    //DBus::MessageIter wi = reply.writer();
    //wi << plugin_settings;
    return reply;
}
void CCommLayerServerDBus::RegisterPlugin(const std::string& pName)
{
    return m_pObserver->RegisterPlugin(pName);
}

DBus::Message CCommLayerServerDBus::_UnRegisterPlugin_stub(const DBus::CallMessage &call)
{
    DBus::MessageIter ri = call.reader();
    std::string PluginName;
    ri >> PluginName;
    UnRegisterPlugin(PluginName);
    DBus::ReturnMessage reply(call);
    return reply;
}
void CCommLayerServerDBus::UnRegisterPlugin(const std::string& pName)
{
    return m_pObserver->UnRegisterPlugin(pName);
}


/*
 * DBus signal emitters
 */

/* Notify the clients (UI) about a new crash */
void CCommLayerServerDBus::Crash(const std::string& arg1)
{
    ::DBus::SignalMessage sig("Crash");
    ::DBus::MessageIter wi = sig.writer();
    wi << arg1;
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

void CCommLayerServerDBus::Error(const std::string& arg1)
{
    ::DBus::SignalMessage sig("Error");
    ::DBus::MessageIter wi = sig.writer();
    wi << arg1;
    emit_signal(sig);
}

void CCommLayerServerDBus::Update(const std::string& pDest, const std::string& pMessage)
{
    ::DBus::SignalMessage sig("Update");
    ::DBus::MessageIter wi = sig.writer();
    wi << pDest;
    wi << pMessage;
    emit_signal(sig);
}

void CCommLayerServerDBus::Warning(const std::string& arg1)
{
    ::DBus::SignalMessage sig("Warning");
    ::DBus::MessageIter wi = sig.writer();
    wi << arg1;
    emit_signal(sig);
}
