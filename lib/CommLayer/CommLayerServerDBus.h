#include "CommLayerServer.h"

#include <dbus-c++/dbus.h>
#include <dbus-c++/glib-integration.h>
#include "DBusServerProxy.h"
#include <iostream>

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
        CCommLayerServerDBus();
        virtual ~CCommLayerServerDBus();

        virtual vector_crash_infos_t GetCrashInfos(const std::string &pSender);
        /*FIXME: fix CLI and remove this stub*/
        virtual map_crash_report_t CreateReport(const std::string &pUUID,const std::string &pSender){map_crash_report_t retval; return retval;};
        virtual uint64_t CreateReport_t(const std::string &pUUID,const std::string &pSender);
        virtual bool Report(map_crash_report_t pReport,const std::string &pSender);
        virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pSender);
        virtual map_crash_report_t GetJobResult(uint64_t pJobID, const std::string& pSender);
        virtual vector_map_string_string_t GetPluginsInfo();
        virtual map_plugin_settings_t GetPluginSettings(const std::string& pName, const std::string& pSender);
        void RegisterPlugin(const std::string& pName);
        void UnRegisterPlugin(const std::string& pName);

        virtual void Crash(const std::string& arg1);
        virtual void AnalyzeComplete(map_crash_report_t arg1);
        virtual void Error(const std::string& arg1);
        virtual void Update(const std::string& pDest, const std::string& pMessage);
        virtual void JobDone(const std::string &pDest, uint64_t pJobID);
};

