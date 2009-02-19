bin_PROGRAMS = applet
applet_SOURCES = Applet.cpp  CCApplet.cpp  CCApplet.h DBusClientProxy.h
applet_CPPFLAGS = -I../Daemon/ \
                  -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
                  $(DBUS_GLIB_CFLAGS) $(GTK_CFLAGS) $(DBUSCPP_CFLAGS) \
                  -I../../lib/MiddleWare -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
                  
applet_LDADD = $(DL_LIBS) $(GTK_LIBS) -lglib-2.0 -lgthread-2.0 $(DBUSCPP_LIBS)
