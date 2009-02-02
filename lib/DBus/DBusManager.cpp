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
#include <marshal.h>

// only for testing - used by LoopSend()
static gboolean send_message(DBusGConnection *con)
{
     DBusMessage *message;
    /* Create a new signal "Crash" on the "com.redhat.CrashCatcher" interface,
    * from the object "/com/redhat/CrashCatcher". */
    message = dbus_message_new_signal("/com/redhat/CrashCatcher/Crash",
                                     "com.redhat.CrashCatcher", "Crash");
    if(!message){
      fprintf(stderr,"Message creating error");
    }
    char *progname = "Foo";
    /* Append some info to the signal */
    dbus_message_append_args(message,DBUS_TYPE_STRING, &progname, DBUS_TYPE_INVALID);
    /* get the DBusConnection */
    DBusConnection *dbus_con = dbus_g_connection_get_connection(con);
    /* Send the signal via low level dbus function coz glib doesn't seem to work */
    if(!dbus_connection_send(dbus_con, message, NULL)){
        printf("Error while sending message\n");
    }
    printf("flushing bus %p\n", dbus_con);
    dbus_connection_flush(dbus_con);
    /* Free the signal */
    dbus_message_unref(message);
    /* Tell the user we send a signal */
    g_print("Message sent!\n");
    /* Return TRUE to tell the event loop we want to be called again */
    return TRUE;
}


CDBusManager::CDBusManager()
{
    GError *error = NULL;
    g_type_init();
    /* first we need to connect to dbus */
    m_nBus = dbus_g_bus_get(DBUS_BUS, &error);
    if(!m_nBus)
        throw std::string("Couldn't connect to dbus") + error->message;
}

CDBusManager::~CDBusManager()
{
}

/* register name com.redhat.CrashCatcher on dbus */
void CDBusManager::RegisterService()
{
    GError *error = NULL;
    guint request_name_result;
    g_type_init();
    // then we need a proxy to talk to dbus
    m_nBus_proxy = dbus_g_proxy_new_for_name(m_nBus, DBUS_SERVICE_DBUS,
                                     DBUS_PATH_DBUS,
                                     DBUS_INTERFACE_DBUS);
    if(!m_nBus_proxy){
        std::cerr << "Error while creating dbus proxy!" << error->message << std::endl;
    }
    /* and register our name */
    if (!dbus_g_proxy_call(m_nBus_proxy, "RequestName", &error,
              G_TYPE_STRING, CC_DBUS_NAME,
              G_TYPE_UINT, 0,
              G_TYPE_INVALID,
              G_TYPE_UINT, &request_name_result,
              G_TYPE_INVALID))
    {
        throw std::string("Failed to acquire com.redhat.CrashCatcher:") + error->message ;
    }
#ifdef DEBUG
    std::cout << "Service running" << std::endl;
#endif
}

void CDBusManager::ConnectToDaemon()
{
    GError *error = NULL;
    guint request_name_result;
    g_type_init();
    /* create a proxy object to talk and listen to CC daemon */
    m_nCCBus_proxy = dbus_g_proxy_new_for_name_owner(m_nBus, CC_DBUS_NAME,
                                                             CC_DBUS_PATH_NOTIFIER,
                                                             CC_DBUS_IFACE, &error);
    if(!m_nCCBus_proxy){
#ifdef DEBUG
      std::cerr << "Couldn't connect to daemon via dbus: " << error->message << std::endl;
#endif
    throw std::string(error->message);
    }
#ifdef DEBUG
    std::cout << "Connected! Waiting for signals\n" << std::endl;
#endif
}

void CDBusManager::RegisterToMessage(const std::string& pMessage, GCallback handler, void * data, GClosureNotify free_data_func)
{
#ifdef DEBUG
    std::cout << "Trying to register" << std::endl;
#endif /*DEBUG*/
    /* Register dbus signal marshaller */
    dbus_g_object_register_marshaller(marshal_VOID__STRING, 
                                      G_TYPE_NONE, G_TYPE_STRING, 
                                      G_TYPE_INVALID);
    dbus_g_proxy_add_signal(m_nCCBus_proxy,"Crash",G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(m_nCCBus_proxy,"Crash",handler,NULL, NULL);
#ifdef DEBUG
    std::cout << "Register done" << std::endl;
#endif /*DEBUG*/
}

bool CDBusManager::GSendMessage(const std::string& pMessage, const std::string& pMessParam)
{
    DBusMessage *message;
    /* Create a new signal "Crash" on the "com.redhat.CrashCatcher" interface,
    * from the object "/com/redhat/CrashCatcher/Crash". */
    message = dbus_message_new_signal ("/com/redhat/CrashCatcher/Crash",
                                     "com.redhat.CrashCatcher", pMessage.c_str());
    if(!message){
      std::cerr << "Message creating error" << std::endl;
    }
#ifdef DEBUG
    std::cerr << message << std::endl;
#endif
    const char *progname = pMessParam.c_str();
    /* Append some info to the signal */
    dbus_message_append_args(message,DBUS_TYPE_STRING, &progname, DBUS_TYPE_INVALID);
    /* Send the signal */
    dbus_g_proxy_send(m_nCCBus_proxy, message, NULL);
    /* Free the signal */
    dbus_message_unref(message);
#ifdef DEBUG
    g_print("Message sent!\n");
#endif
    /* Return TRUE to tell the event loop we want to be called again */
    return TRUE;
}

bool CDBusManager::SendMessage(const std::string& pMessage, const std::string& pMessParam)
{
    DBusMessage *message;
    const char *progname = pMessParam.c_str();
    /* Create a new signal "Crash" on the "com.redhat.CrashCatcher" interface,
    * from the object "/com/redhat/CrashCatcher/Crash". */
    message = dbus_message_new_signal ("/com/redhat/CrashCatcher/Crash",
                                       "com.redhat.CrashCatcher", pMessage.c_str());
    if(!message){
      std::cerr << "Message creating error" << std::endl;
    }
#ifdef DEBUG
    std::cerr << message << std::endl;
#endif
    /* Add program name as the message argument */
    dbus_message_append_args(message,DBUS_TYPE_STRING, &progname, DBUS_TYPE_INVALID);
    /* Send the signal */
    DBusConnection *dbus_con = dbus_g_connection_get_connection(m_nBus);
    /* Send the signal via low level dbus functio coz glib sucks */
    if(!dbus_connection_send(dbus_con, message, NULL)){
        throw "Error while sending message";
    }
    /* flush the connection, otherwise, it won't show on dbus? */
    dbus_connection_flush(dbus_con);
    /* Free the signal */
    dbus_message_unref(message);
#ifdef DEBUG
    g_print("Message sent!\n");
#endif
    /* Return TRUE to tell the event loop we want to be called again */
    return TRUE;
}


// just for testing purposes
void CDBusManager::LoopSend()
{
    g_timeout_add(1000, (GSourceFunc)send_message, m_nBus);
}

void CDBusManager::Unregister()
{
    std::cerr << "Unregister" << std::endl;
}
