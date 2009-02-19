bin_PROGRAMS = crash-catcher
crash_catcher_SOURCES = CrashWatcher.cpp CrashWatcher.h Daemon.cpp DBusServerProxy.h
crash_catcher_CPPFLAGS = -I../../lib/MiddleWare\
                        -I../../lib/DBus \
                        -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" $(DBUS_GLIB_CFLAGS) $(DBUSCPP_CFLAGS) \
                        -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
                        -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
                        -DCONF_DIR=\"$(CONF_DIR)\"
crash_catcher_LDADD = ../../lib/MiddleWare/libMiddleWare.la $(DL_LIBS) $(DBUSCPP_LIBS) $(RPM_LIBS)
