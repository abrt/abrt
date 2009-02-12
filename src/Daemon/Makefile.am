bin_PROGRAMS = CrashCatcher
CrashCatcher_SOURCES = CrashWatcher.cpp CrashWatcher.h Daemon.cpp
CrashCatcher_CPPFLAGS = -I../../lib/MiddleWare\
                        -I../../lib/DBus \
                        -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" $(DBUS_GLIB_CFLAGS) $(DBUSCPP_CFLAGS) \
                        -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
                        -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
                        -DCONF_DIR=\"$(CONF_DIR)\"
CrashCatcher_LDADD = ../../lib/MiddleWare/libMiddleWare.la ../../lib/DBus/libDBus.la $(DL_LIBS)
