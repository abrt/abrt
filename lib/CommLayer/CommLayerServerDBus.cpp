#include "CommLayerServerDBus.h"
#include <iostream>

DBus::Connection *CCommLayerServerDBus::init_dbus(CCommLayerServerDBus *self)
{
    CCommLayerServerDBus *server = (CCommLayerServerDBus*) self;
    server->dispatcher = new DBus::Glib::BusDispatcher();
    server->dispatcher->attach(NULL);
    DBus::default_dispatcher = self->dispatcher;
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
    delete dispatcher;
}

dbus_vector_crash_infos_t CCommLayerServerDBus::GetCrashInfos(const std::string &pUID)
{
    dbus_vector_crash_infos_t retval;
    vector_crash_infos_t crash_info;
    m_pMW->GetCrashInfos("501");
    for (vector_crash_infos_t::iterator it = crash_info.begin(); it!=crash_info.end(); ++it) {
        std::cerr << it->m_sExecutable << std::endl;
    }
	return retval;
}

dbus_vector_map_crash_infos_t CCommLayerServerDBus::GetCrashInfosMap(const std::string &pDBusSender)
{
    dbus_vector_map_crash_infos_t retval;
    vector_crash_infos_t crash_info;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pDBusSender.c_str());
    try
    {
        crash_info = m_pMW->GetCrashInfos(to_string(unix_uid));
    }
    catch(std::string err)
    {
        std::cerr << err << std::endl;
    }
    for (vector_crash_infos_t::iterator it = crash_info.begin(); it!=crash_info.end(); ++it) {
        std::cerr << it->m_sExecutable << std::endl;
        retval.push_back(it->GetMap());
    }
    Notify("Sent crash info");
	return retval;
}

dbus_vector_crash_report_info_t CCommLayerServerDBus::CreateReport(const std::string &pUUID,const std::string &pDBusSender)
{
    dbus_vector_crash_report_info_t retval;
    unsigned long unix_uid = m_pConn->sender_unix_uid(pDBusSender.c_str());
    //std::cerr << pUUID << ":" << unix_uid << std::endl;
    crash_report_t crashReport;
    std::cerr << "Creating report" << std::endl;
    try
    {
        m_pMW->CreateCrashReport(pUUID,to_string(unix_uid), crashReport);
        retval = crash_report_to_vector_strings(crashReport);
        //send out the message about completed analyze
        CDBusServer_adaptor::AnalyzeComplete(retval);
    }
    catch(std::string err)
    {
        CDBusServer_adaptor::Error(err);
        std::cerr << err << std::endl;
    }
    return retval;
}

bool CCommLayerServerDBus::Report(dbus_vector_crash_report_info_t pReport)
{
    crash_report_t crashReport = vector_strings_to_crash_report(pReport);
    //#define FIELD(X) crashReport.m_s##X = pReport[#X];
    //crashReport.m_sUUID = pReport["UUID"];
    //ALL_CRASH_REPORT_FIELDS;
    //#undef FIELD
    //for (dbus_map_report_info_t::iterator it = pReport.begin(); it!=pReport.end(); ++it) {
    //     std::cerr << it->second << std::endl;
    //}
    try
    {
        m_pMW->Report(crashReport);
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
