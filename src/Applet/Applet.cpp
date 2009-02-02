#include "CCApplet.h"
#include "DBusManager.h"
#include <iostream>

//@@global applet object
CApplet *applet;
static void
crash_notify_cb(DBusGProxy *proxy, char* progname, gpointer user_data)
{
    DBusError error;
    dbus_error_init (&error);
#ifdef DEBUG
    std::cerr << "Application " << progname << " has crashed!" << std::endl;
#endif
    /* smth happend, show the blinking icon */
    applet->BlinkIcon(true);
    applet->ShowIcon();
}

int main(int argc, char **argv)
{
    Gtk::Main kit(argc, argv);
    applet = new CApplet();
    CDBusManager dm;
    /* connect to the daemon */
    try
    {
        dm.ConnectToDaemon();
    }
    catch(std::string err)
    {
        std::cerr << "Applet: " << err << std::endl;
        return -1;
    }
    /* catch the CC crash notify on the dbus */
    dm.RegisterToMessage("Crash",G_CALLBACK(crash_notify_cb),NULL,NULL);
    /* run the main loop and wait for some events */
    Gtk::Main::run();
    return 0;
}
