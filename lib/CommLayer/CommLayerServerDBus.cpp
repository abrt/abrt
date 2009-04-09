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

CCommLayerServerDBus::CCommLayerServerDBus(CMiddleWare *pMW)
: CCommLayerServer(pMW),
  DBus::ObjectAdaptor(*init_dbus(this), CC_DBUS_PATH)
{
    std::cerr << "CCommLayerDBus init.." << std::endl;
    m_pConn->request_name(CC_DBUS_NAME);

}

CCommLayerServerDBus::~CCommLayerServerDBus()
{
    std::cout << "Cleaning up dbus" << std::endl;
    delete m_pDispatcher;
}

vector_crash_infos_t CCommLayerServerDBus::GetCrashInfos(const std::string &pDBusSender)
{
    vector_crash_infos_t retval;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pDBusSender.c_str());
    try
    {
        retval = m_pMW->GetCrashInfos(to_string(unix_uid));
    }
    catch(std::string err)
    {
        std::cerr << err << std::endl;
    }
    Notify("Sent crash info");
	return retval;
}

map_crash_report_t CCommLayerServerDBus::CreateReport(const std::string &pUUID,const std::string &pDBusSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pDBusSender.c_str());
    //std::cerr << pUUID << ":" << unix_uid << std::endl;
    map_crash_report_t crashReport;
    std::cerr << "Creating report" << std::endl;
    try
    {
        m_pMW->CreateCrashReport(pUUID,to_string(unix_uid), crashReport);
        //send out the message about completed analyze
        CDBusServer_adaptor::AnalyzeComplete(crashReport);
    }
    catch(std::string err)
    {
        CDBusServer_adaptor::Error(err);
        std::cerr << err << std::endl;
    }
    return crashReport;
}

bool CCommLayerServerDBus::Report(map_crash_report_t pReport)
{
    //#define FIELD(X) crashReport.m_s##X = pReport[#X];
    //crashReport.m_sUUID = pReport["UUID"];
    //ALL_CRASH_REPORT_FIELDS;
    //#undef FIELD
    //for (dbus_map_report_info_t::iterator it = pReport.begin(); it!=pReport.end(); ++it) {
    //     std::cerr << it->second << std::endl;
    //}
    try
    {
        m_pMW->Report(pReport);
    }
    catch(std::string err)
    {
        std::cerr << err << std::endl;
        return false;
    }
    return true;
}

bool CCommLayerServerDBus::DeleteDebugDump(const std::string& pUUID, const std::string& pDBusSender)
{
    unsigned long unix_uid = m_pConn->sender_unix_uid(pDBusSender.c_str());
    try
    {
        //std::cerr << "DeleteDebugDump(" << pUUID << "," << unix_uid << ")" << std::endl;
        m_pMW->DeleteCrashInfo(pUUID,to_string(unix_uid), true);
    }
    catch(std::string err)
    {
        std::cerr << err << std::endl;
        return false;
    }
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
