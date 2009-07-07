#include <DBusCommon.h>
#include <DBusClientProxy.h>
#include <iostream>

class CCommLayerClientDBus
: public CDBusClient_proxy,
  public DBus::IntrospectableProxy,
  public DBus::ObjectProxy
{
    public:
        CCommLayerClientDBus(DBus::Connection &connection, const char *path, const char *name);
        ~CCommLayerClientDBus();
        void Crash(std::string &value)
        {
            std::cout << "Another Crash?" << std::endl;
        }
};
