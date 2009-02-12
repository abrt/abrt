bin_PROGRAMS = applet
applet_SOURCES = Applet.cpp  CCApplet.cpp  CCApplet.h
applet_CPPFLAGS = -I../../lib/DBus \
                  -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
                  $(DBUS_GLIB_CFLAGS) $(GTKMM_CFLAGS) $(DBUSCPP_CFLAGS) \
                  -I../../lib/MiddleWare
                  
applet_LDADD = ../../lib/DBus/libDBus.la $(DL_LIBS) $(GTKMM_LIBS)
