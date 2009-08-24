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
: CCommLayerServer(),
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
}

CCommLayerServerDBus::~CCommLayerServerDBus()
{
}

vector_crash_infos_t CCommLayerServerDBus::GetCrashInfos(const std::string &pSender)
{
    vector_crash_infos_t crashInfos;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    crashInfos = m_pObserver->GetCrashInfos(to_string(unix_uid));
    return crashInfos;
}
//FIXME: fix CLI and remove this
/*
map_crash_report_t CCommLayerServerDBus::CreateReport(const std::string &pUUID,const std::string &pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    map_crash_report_t crashReport;
    crashReport = m_pObserver->CreateReport(pUUID, to_string(unix_uid));
    return crashReport;
}
*/
uint64_t CCommLayerServerDBus::CreateReport_t(const std::string &pUUID,const std::string &pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    map_crash_report_t crashReport;
    uint64_t job_id = m_pObserver->CreateReport_t(pUUID, to_string(unix_uid), pSender);
    return job_id;
}

report_status_t CCommLayerServerDBus::Report(map_crash_report_t pReport,const std::string &pSender)
{
    report_status_t rs;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    rs = m_pObserver->Report(pReport, to_string(unix_uid));
    return rs;
}

bool CCommLayerServerDBus::DeleteDebugDump(const std::string& pUUID, const std::string& pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    m_pObserver->DeleteDebugDump(pUUID,to_string(unix_uid));
    return true;
}

map_crash_report_t CCommLayerServerDBus::GetJobResult(uint64_t pJobID, const std::string& pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    map_crash_report_t crashReport;
    crashReport = m_pObserver->GetJobResult(pJobID,to_string(unix_uid));
    return crashReport;
}

void CCommLayerServerDBus::Crash(const std::string& arg)
{
    CDBusServer_adaptor::Crash(arg);
}

void CCommLayerServerDBus::AnalyzeComplete(map_crash_report_t arg1)
{
    CDBusServer_adaptor::AnalyzeComplete(arg1);
}

void CCommLayerServerDBus::Error(const std::string& arg1)
{
    CDBusServer_adaptor::Error(arg1);
}

void CCommLayerServerDBus::Update(const std::string& pDest, const std::string& pMessage)
{
    CDBusServer_adaptor::Update(pDest, pMessage);
}

void CCommLayerServerDBus::JobDone(const std::string &pDest, uint64_t pJobID)
{
    CDBusServer_adaptor::JobDone(pDest, pJobID);
}

void CCommLayerServerDBus::Warning(const std::string& pDest, const std::string& pMessage)
{
    CDBusServer_adaptor::Warning(pMessage);
}

vector_map_string_string_t CCommLayerServerDBus::GetPluginsInfo()
{
    //FIXME: simplify?
    vector_map_string_string_t plugins_info;
    plugins_info = m_pObserver->GetPluginsInfo();
    return plugins_info;
}

map_plugin_settings_t CCommLayerServerDBus::GetPluginSettings(const std::string& pName, const std::string& pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    return m_pObserver->GetPluginSettings(pName, to_string(unix_uid));
}

void CCommLayerServerDBus::RegisterPlugin(const std::string& pName)
{
    return m_pObserver->RegisterPlugin(pName);
}

void CCommLayerServerDBus::UnRegisterPlugin(const std::string& pName)
{
    return m_pObserver->UnRegisterPlugin(pName);
}

void CCommLayerServerDBus::SetPluginSettings(const std::string& pName, const std::string& pSender, const map_plugin_settings_t& pSettings)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    return m_pObserver->SetPluginSettings(pName, to_string(unix_uid), pSettings);
}

