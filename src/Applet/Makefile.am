bin_PROGRAMS = abrt-applet
abrt_applet_SOURCES = Applet.cpp  CCApplet.cpp  CCApplet.h DBusClientProxy.h
abrt_applet_CPPFLAGS = -Wall -Werror -I../Daemon/ \
                       -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
                       $(DBUS_GLIB_CFLAGS) $(GTK_CFLAGS) $(DBUSCPP_CFLAGS) $(LIBNOTIFY_CFLAGS)\
                       -I../../lib/MiddleWare -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
                  
abrt_applet_LDADD = $(DL_LIBS) $(GTK_LIBS) -lglib-2.0 -lgthread-2.0 $(DBUSCPP_LIBS) $(LIBNOTIFY_LIBS)

EXTRA_DIST = abrt-applet.desktop

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = abrt-applet.desktop
