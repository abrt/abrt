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

#include "DBusManager.h"
#include <iostream>
#include "marshal.h"

CDBusManager::CDBusManager()
{
    DBus::default_dispatcher = new DBus::Glib::BusDispatcher();
    m_pConn = new DBus::Connection(DBus::Connection::SessionBus());
    
}

CDBusManager::~CDBusManager()
{
    //delete DBus::default_dispatcher
    // delete m_pConn
}

/* register name com.redhat.CrashCatcher on dbus */
void CDBusManager::RegisterService()
{
    /* this can lead to race condition - rewrite with better check */
    if(m_pConn->has_name(CC_DBUS_NAME))
    {
        /* shouldn't happen, but ... */
        throw std::string("Name already taken: ") + CC_DBUS_NAME;
    }
    /* register our name */
    m_pConn->request_name(CC_DBUS_NAME,DBUS_NAME_FLAG_DO_NOT_QUEUE);
#ifdef DEBUG
    std::cout << "Service running" << std::endl;
#endif
}

bool CDBusManager::SendMessage(const std::string& pMessage, const std::string& pMessParam)
{
    const char *progname = pMessParam.c_str();
    DBus::SignalMessage mess(CC_DBUS_PATH, CC_DBUS_NAME, pMessage.c_str());
    /* append some info to the signal */
    mess.append(DBUS_TYPE_STRING,&progname,DBUS_TYPE_INVALID);
    /* send the message */
    m_pConn->send(mess);
    /* flush - to make sure it's not stuck in queue */
    // probably not needed
    //m_pConn->flush();
    std::cerr << "Message sent!" << std::endl;
    return TRUE;
}
