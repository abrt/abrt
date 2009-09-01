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
#include <iostream>
#include <dbus/dbus-shared.h>

#if HAVE_CONFIG_H
    #include <config.h>
#endif

#if HAVE_LOCALE_H
    #include <locale.h>
#endif

#if ENABLE_NLS
    #include <libintl.h>
    #define _(S) gettext(S)
#else
    #define _(S) (S)
#endif

//@@global applet object
CApplet *applet;

static void
crash_notify_cb(const char* progname)
{
#ifdef DEBUG
    std::cerr << "Application " << progname << " has crashed!" << std::endl;
#endif
    //applet->AddEvent(uid, std::string(progname));
    applet->SetIconTooltip(_("A crash in package %s has been detected!"), progname);
    applet->ShowIcon();
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL,"");

#if ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    /* need to be thread safe */
    g_thread_init(NULL);
    gdk_threads_init();
    gdk_threads_enter();
    gtk_init(&argc,&argv);
    /* prevent zombies when we spawn abrt-gui */
    signal(SIGCHLD, SIG_IGN);

    /* move to the DBusClient::connect */
    DBus::Glib::BusDispatcher dispatcher;
    /* this should bind the dispatcher with mainloop */
    dispatcher.attach(NULL);
    DBus::default_dispatcher = &dispatcher;
    DBus::Connection session = DBus::Connection::SessionBus();
    //FIXME: possible race, but the dbus-c++ API won't let us check return value of request_name :(
    if(session.has_name("com.redhat.abrt.applet"))
    {
        //applet is already running
        std::cerr << _("Applet is already running.") << std::endl;
        return -1;
    }
    else
    {
        //applet is not running, so claim the name on the session bus
        session.request_name("com.redhat.abrt.applet");
    }

    DBus::Connection conn = DBus::Connection::SystemBus();
    applet = new CApplet(conn, CC_DBUS_PATH, CC_DBUS_NAME);
    applet->ConnectCrashHandler(crash_notify_cb);
    if(!conn.has_name(CC_DBUS_NAME))
    {
        std::cout << _("ABRT service is not running") << std::endl;
        applet->Disable(_("ABRT service is not running"));
    }

    gtk_main();
    gdk_threads_leave();
    delete applet;
    return 0;
}
