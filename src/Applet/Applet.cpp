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
    
#include "CCApplet.h"
#include "DBusClient.h"
#include <iostream>

//@@global applet object
CApplet *applet;
static void
crash_notify_cb(const char* progname)
{
#ifdef DEBUG
    std::cerr << "Application " << progname << " has crashed!" << std::endl;
#endif
    /* smth happend, show the blinking icon */
    applet->BlinkIcon(true);
    applet->ShowIcon();
    //applet->AddEvent(uid, std::string(progname));
    applet->SetIconTooltip("A crash in package %s has been detected!", progname);
}

int main(int argc, char **argv)
{
    /* need to be thread safe */
    g_thread_init(NULL);
    gdk_threads_init();
    gdk_threads_enter();
    gtk_init(&argc,&argv);
    applet = new CApplet();
    /* move to the DBusClient::connect */
    DBus::Glib::BusDispatcher dispatcher;
    /* this should bind the dispatcher with mainloop */
    dispatcher.attach(NULL);
    DBus::default_dispatcher = &dispatcher;

	DBus::Connection conn = DBus::Connection::SystemBus();
    CDBusClient client(conn, CC_DBUS_PATH, CC_DBUS_NAME);
    client.ConnectCrashHandler(crash_notify_cb);
    gtk_main();
    gdk_threads_leave();
    return 0;
}
