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
        static DBus::Connection *init_dbus(CCommLayerServerDBus *self);
    public:
        CCommLayerServerDBus();
        virtual ~CCommLayerServerDBus();

        /* DBus call handlers */
        virtual vector_crash_infos_t GetCrashInfos(const std::string& pSender);
        virtual uint64_t CreateReport_t(const std::string& pUUID, const std::string& pSender);
        virtual report_status_t Report(const map_crash_report_t& pReport, const std::string& pSender);
        virtual bool DeleteDebugDump(const std::string& pUUID, const std::string& pSender);
        virtual map_crash_report_t GetJobResult(uint64_t pJobID, const std::string& pSender);
        virtual vector_map_string_string_t GetPluginsInfo();
        virtual map_plugin_settings_t GetPluginSettings(const std::string& pName, const std::string& pSender);
        virtual void SetPluginSettings(const std::string& pName, const std::string& pSender, const map_plugin_settings_t& pSettings);
        virtual void RegisterPlugin(const std::string& pName);
        virtual void UnRegisterPlugin(const std::string& pName);

        /* Double duty: */
        /* (1) implement CCommLayerServer's virtuals */
        /* (2) propagate values to CDBusServer_adaptor::<same_name>() in order to send a DBus signal */
        virtual void Crash(const std::string& arg1);
        virtual void AnalyzeComplete(const map_crash_report_t& arg1);
        virtual void Error(const std::string& arg1);
        virtual void Update(const std::string& pDest, const std::string& pMessage);
        virtual void JobDone(const std::string& pDest, uint64_t pJobID);
        virtual void Warning(const std::string& pDest, const std::string& pMessage);
};

/*
 * This must be done before instances of CCommLayerServerDBus are created
 * (otherwise "new DBus::Connection(DBus::Connection::SystemBus())" fails)
 */
void attach_dbus_dispatcher_to_glib_main_context();
