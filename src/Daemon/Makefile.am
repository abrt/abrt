sbin_PROGRAMS = abrt
abrt_SOURCES = CrashWatcher.cpp CrashWatcher.h Daemon.cpp DBusServerProxy.h \
                        DBusCommon.h Settings.h Settings.cpp
abrt_CPPFLAGS = -I$(srcdir)/../../lib/MiddleWare -I$(srcdir)/../../lib/CommLayer\
                        -I$(srcdir)/../../inc -I$(srcdir)/../../lib/Utils\
                        -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" $(GLIB_CFLAGS) $(DBUSCPP_CFLAGS) \
                        -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
                        -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
                        -DCONF_DIR=\"$(CONF_DIR)\" \
                        -DVAR_RUN=\"$(VAR_RUN)\" \
                        $(ENABLE_SOCKET_OR_DBUS)
abrt_LDADD = ../../lib/MiddleWare/libABRTMiddleWare.la ../../lib/CommLayer/libABRTCommLayer.la $(DL_LIBS) $(DBUSCPP_LIBS) $(RPM_LIBS)
dbusabrtconfdir = ${sysconfdir}/dbus-1/system.d/
dist_dbusabrtconf_DATA = dbus-abrt.conf

daemonconfdir = $(CONF_DIR)
dist_daemonconf_DATA = abrt.conf

man_MANS = abrt.8 abrt.conf.5
EXTRA_DIST = $(man_MANS)
