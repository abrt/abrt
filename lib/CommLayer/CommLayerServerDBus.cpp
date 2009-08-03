#include "CommLayerServerDBus.h"
#include <iostream>
#include "ABRTException.h"

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
        throw CABRTException(EXCEP_FATAL, "CCommLayerServerDBus::CCommLayerServerDBus(): Error while requesting dbus name - have you reloaded the dbus settings?");
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

bool CCommLayerServerDBus::Report(map_crash_report_t pReport,const std::string &pSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pSender.c_str());
    m_pObserver->Report(pReport, to_string(unix_uid));
    return true;
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
