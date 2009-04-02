#include "CommLayerServer.h"

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>
#include "DBusServerProxy.h"

class CCommLayerServerDBus
: public CCommLayerServer,
  public CDBusServer_adaptor,
  public DBus::IntrospectableAdaptor,
  public DBus::ObjectAdaptor
{
    private:
        DBus::Connection *m_pConn;
        DBus::Glib::BusDispatcher *m_pDispatcher;
        static DBus::Connection *init_dbus(CCommLayerServerDBus *self);
    public:
        CCommLayerServerDBus(CMiddleWare *m_pMW);
        virtual ~CCommLayerServerDBus();

        virtual dbus_vector_crash_infos_t GetCrashInfos(const std::string &pUID);
        virtual dbus_vector_map_crash_infos_t GetCrashInfosMap(const std::string &pDBusSender);
        virtual dbus_vector_crash_report_info_t CreateReport(const std::string &pUUID,const std::string &pDBusSender);
        virtual bool Report(dbus_vector_crash_report_info_t pReport);
        virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pDBusSender);
};

