#include "CommLayerServerDBus.h"
#include <iostream>

DBus::Connection *CCommLayerServerDBus::init_dbus(CCommLayerServerDBus *self)
{
    CCommLayerServerDBus *server = (CCommLayerServerDBus*) self;
    server->m_pDispatcher = new DBus::Glib::BusDispatcher();
    server->m_pDispatcher->attach(NULL);
    DBus::default_dispatcher = self->m_pDispatcher;
	server->m_pConn = new DBus::Connection(DBus::Connection::SystemBus());
    return server->m_pConn;
}

CCommLayerServerDBus::CCommLayerServerDBus()
: CCommLayerServer(),
  DBus::ObjectAdaptor(*init_dbus(this), CC_DBUS_PATH)
{
    try
    {
        m_pConn->request_name(CC_DBUS_NAME);
    }
    catch(DBus::Error err)
    {
        throw std::string("Error while requesting dbus name - have you reloaded the dbus settings?");
    }

}

CCommLayerServerDBus::~CCommLayerServerDBus()
{
    delete m_pDispatcher;
}

vector_crash_infos_t CCommLayerServerDBus::GetCrashInfos(const std::string &pSender)
{
    vector_crash_infos_t crashInfos;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    crashInfos = m_pObserver->GetCrashInfos(to_string(unix_uid));
	return crashInfos;
}

map_crash_report_t CCommLayerServerDBus::CreateReport(const std::string &pUUID,const std::string &pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    map_crash_report_t crashReport;
    crashReport = m_pObserver->CreateReport(pUUID, to_string(unix_uid));
    return crashReport;
}

bool CCommLayerServerDBus::Report(map_crash_report_t pReport)
{
    m_pObserver->Report(pReport);
    return true;
}

bool CCommLayerServerDBus::DeleteDebugDump(const std::string& pUUID, const std::string& pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    m_pObserver->DeleteDebugDump(pUUID,to_string(unix_uid));
    return true;
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
