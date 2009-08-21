sbin_PROGRAMS = abrt

abrt_SOURCES = \
    DBusServerProxy.h DBusServerProxy.cpp \
    DynamicLibrary.cpp DynamicLibrary.h \
    ABRTPlugin.cpp ABRTPlugin.h \
    PluginManager.cpp PluginManager.h \
    MiddleWare.cpp MiddleWare.h \
    CrashWatcher.cpp CrashWatcher.h \
    CommLayerServer.h CommLayerServer.cpp \
    CommLayerServerSocket.h CommLayerServerSocket.cpp \
    CommLayerServerDBus.h CommLayerServerDBus.cpp \
    Daemon.cpp \
    Settings.h Settings.cpp
abrt_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/MiddleWare \
    -I$(srcdir)/../../lib/CommLayer \
    -I$(srcdir)/../../lib/Utils \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) $(DBUSCPP_CFLAGS) \
    $(ENABLE_SOCKET_OR_DBUS)
abrt_LDADD = \
    ../../lib/MiddleWare/libABRTMiddleWare.la \
    ../../lib/CommLayer/libABRTCommLayer.la \
    $(DL_LIBS) $(DBUSCPP_LIBS) $(RPM_LIBS)

dbusabrtconfdir = ${sysconfdir}/dbus-1/system.d/
dist_dbusabrtconf_DATA = dbus-abrt.conf

daemonconfdir = $(CONF_DIR)
dist_daemonconf_DATA = abrt.conf

man_MANS = abrt.8 abrt.conf.5
EXTRA_DIST = $(man_MANS)

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(VAR_RUN)'
