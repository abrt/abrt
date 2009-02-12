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
    
#include "DBusServer.h"

CDBusServer::CDBusServer(DBus::Connection &connection, const std::string& pServerPath)
: DBus::ObjectAdaptor(connection, pServerPath)
{
}

dbus_vector_crash_infos_t CDBusServer::GetCrashInfos(const std::string &pUID)
{
    dbus_vector_crash_infos_t info;
    //CMiddleWare mw;
    //mw.GetCrashInfos();
    std::vector<std::string> v;
    v.push_back("vector1_prvek1");
    v.push_back("vector1_prvek_1");
    info.push_back(v);
    v.clear();
    v.push_back("vector2_prvek1");
    v.push_back("vector2_prvek_1");
    v.push_back(pUID);
    info.push_back(v);
	return info;
}

CDBusServer::~CDBusServer()
{
    //clean
}
