/* 
    Copyright (C) 2009  Jiri Moskovcak (jmoskovc@redhat.com) 
    Copyright (C) 2009  RedHat inc. 
 
    This program is free software; you can redistribute it and/or modify 
    it under the terms of the GNU General Public License as published by 
    the Free Software Foundation; either version 2 of the License, or 
    (at your option) any later version. 
 
    This program is distributed in the hope that it will be useful, 
    but WITHOUT ANY WARRANTY; without even the implied warranty of 
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
    GNU General Public License for more details. 
 
    You should have received a copy of the GNU General Public License along 
    with this program; if not, write to the Free Software Foundation, Inc., 
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. 
    */
    
#include "DBusClient.h"
#include <iostream>
#include <signal.h>

DBus::BusDispatcher dispatcher;

void niam(int sig)
{
	dispatcher.leave();
}

int main()
{
	signal(SIGTERM, niam);
	signal(SIGINT, niam);

	DBus::default_dispatcher = &dispatcher;

	DBus::Connection conn = DBus::Connection::SessionBus();

    CDBusClient client(conn, CC_DBUS_PATH, CC_DBUS_NAME);
    /*
    typedef std::vector< std::vector<std::string> > type_t;
    type_t vec = client.GetCrashInfos("client");
    for (type_t::iterator it = vec.begin(); it!=vec.end(); ++it) {
        for (std::vector<std::string>::iterator itt = it->begin(); itt!=it->end(); ++itt) {
            std::cout << *itt << std::endl;
        }
    }
    */
    dbus_vector_crash_infos_t v = client.GetCrashInfos("client");
    for (dbus_vector_crash_infos_t::iterator it = v.begin(); it!=v.end(); ++it) {
        for (dbus_vector_crash_infos_t::value_type::iterator itt = it->begin(); itt!=it->end(); ++itt) {
            std::cout << *itt << std::endl;
        }
    }
	dispatcher.enter();

    std::cout << "terminating" << std::endl;

	return 0;
}
