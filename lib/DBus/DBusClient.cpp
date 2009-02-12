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

CDBusClient::CDBusClient(DBus::Connection &connection, const char *path, const char *name)
: DBus::ObjectProxy(connection, path, name)
{
    m_pCrashHandler = NULL;
    std::cerr << "Client created" << std::endl;
}

CDBusClient::~CDBusClient()
{
    //clean
}

void CDBusClient::Crash(std::string &value)
{
    if(m_pCrashHandler)
    {
        m_pCrashHandler(value.c_str());
    }
    else
    {
        std::cout << "This is default handler, you should register your own with ConnectCrashHandler" << std::endl;
        std::cout.flush();
    }
}

void CDBusClient::ConnectCrashHandler(void (*pCrashHandler)(const char *progname))
{
    m_pCrashHandler = pCrashHandler;
}
