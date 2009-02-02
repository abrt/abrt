bin_PROGRAMS = CrashCatcher
CrashCatcher_SOURCES = CrashWatcher.cpp CrashWatcher.h Daemon.cpp
CrashCatcher_CPPFLAGS = -I../../lib/MiddleWare \
			-I../../lib/DBus \
                        -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" $(DBUS_GLIB_CFLAGS)
CrashCatcher_LDADD = ../../lib/MiddleWare/libMiddleWare.la ../../lib/DBus/libDBus.la $(DL_LIBS)
