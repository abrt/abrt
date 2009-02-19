bin_PROGRAMS = applet
applet_SOURCES = Applet.cpp  CCApplet.cpp  CCApplet.h
applet_CPPFLAGS = -I../../lib/DBus \
                  -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
                  $(DBUS_GLIB_CFLAGS) $(GTK_CFLAGS) $(DBUSCPP_CFLAGS) \
                  -I../../lib/MiddleWare -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include
                  
applet_LDADD = ../../lib/DBus/libDBus.la $(DL_LIBS) $(GTK_LIBS) -lglib-2.0 -lgthread-2.0
