#include "CommLayerClientDBus.h"
CCommLayerClientDBus::CCommLayerClientDBus(DBus::Connection &connection, const char *path, const char *name)
: DBus::ObjectProxy(connection, path, name)
{
}
CCommLayerClientDBus::~CCommLayerClientDBus()
{
};

void Crash(std::string &value)
{
    std::cout << "Another Crash?" << std::endl;
}
