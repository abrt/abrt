#include <dbus-c++/dbus.h>
#include "DBusClientProxy.h"

class CDBusClient
: public org::freedesktop::DBus::CDBusClient_proxy,
  public DBus::IntrospectableProxy,
  public DBus::ObjectProxy
{
public:

	CDBusClient(DBus::Connection &connection, const char *path, const char *name);
    ~CDBusClient();
	void Crash(const DBus::Variant &value);
};

