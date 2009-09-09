sbin_PROGRAMS = abrtd

# FIXME: we still link against dbus-c++ (not just dbus)
# because lib/CommLayer links it in

abrtd_SOURCES = \
    ABRTPlugin.cpp ABRTPlugin.h \
    PluginManager.cpp PluginManager.h \
    RPM.cpp RPM.h \
    MiddleWare.cpp MiddleWare.h \
    CrashWatcher.cpp CrashWatcher.h \
    CommLayerServer.h CommLayerServer.cpp \
    CommLayerServerSocket.h CommLayerServerSocket.cpp \
    CommLayerServerDBus.h CommLayerServerDBus.cpp \
    Daemon.cpp Daemon.h \
    Settings.h Settings.cpp \
    Polkit.h Polkit.cpp
abrtd_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/MiddleWare \
    -I$(srcdir)/../../lib/CommLayer \
    -I$(srcdir)/../../lib/Utils \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) $(DBUS_CFLAGS) \
    $(ENABLE_SOCKET_OR_DBUS) $(POLKIT_CFLAGS)
abrtd_LDADD = \
    ../../lib/MiddleWare/libABRTMiddleWare.la \
    ../../lib/CommLayer/libABRTCommLayer.la \
    $(DL_LIBS) $(DBUS_LIBS) $(RPM_LIBS) $(POLKIT_LIBS)

dbusabrtconfdir = ${sysconfdir}/dbus-1/system.d/
dist_dbusabrtconf_DATA = dbus-abrt.conf

daemonconfdir = $(CONF_DIR)
dist_daemonconf_DATA = abrt.conf

polkitconfdir = ${datadir}/polkit-1/actions
dist_polkitconf_DATA = org.fedoraproject.abrt.policy

man_MANS = abrt.8 abrt.conf.5
EXTRA_DIST = $(man_MANS)

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(VAR_RUN)'
