#include "CommLayerClientDBus.h"
CCommLayerClientDBus::CCommLayerClientDBus(DBus::Connection &connection, const char *path, const char *name)
: CDBusClient_proxy(connection),
  DBus::ObjectProxy(connection, path, name)
{
}
CCommLayerClientDBus::~CCommLayerClientDBus()
{
};

void Crash(std::string &value)
{
    std::cout << "Another Crash?" << std::endl;
}
