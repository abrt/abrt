bin_SCRIPTS = \
    abrt-handle-upload

bin_PROGRAMS = \
    abrt-handle-crashdump \
    abrt-action-save-package-data

sbin_PROGRAMS = \
    abrtd \
    abrt-server

abrtd_SOURCES = \
    MiddleWare.h MiddleWare.c \
    CommLayerServerDBus.h CommLayerServerDBus.c \
    comm_layer_inner.h comm_layer_inner.c \
    abrtd.c
abrtd_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DLIBEXEC_DIR=\"$(LIBEXEC_DIR)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings
abrtd_LDADD = \
    $(DBUS_LIBS) \
    ../lib/libreport.la \
    ../lib/libabrt_dbus.la

abrt_server_SOURCES = \
    abrt-server.c
abrt_server_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_server_LDADD = \
    ../lib/libreport.la

abrt_handle_crashdump_SOURCES = \
    abrt-handle-crashdump.c
abrt_handle_crashdump_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DLIBEXEC_DIR=\"$(LIBEXEC_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_handle_crashdump_LDADD = \
    ../lib/libreport.la

abrt_action_save_package_data_SOURCES = \
    rpm.h rpm.c \
    abrt-action-save-package-data.c
abrt_action_save_package_data_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_save_package_data_LDADD = \
    $(RPM_LIBS) \
    ../lib/libreport.la

dbusabrtconfdir = ${sysconfdir}/dbus-1/system.d/
dist_dbusabrtconf_DATA = dbus-abrt.conf

daemonconfdir = $(CONF_DIR)
dist_daemonconf_DATA = \
    abrt.conf \
    abrt_event.conf \
    gpg_keys

comredhatabrtservicedir = ${datadir}/dbus-1/system-services
dist_comredhatabrtservice_DATA = com.redhat.abrt.service

man_MANS = \
    abrtd.8 \
    abrt.conf.5 \
    abrt_event.conf.5

EXTRA_DIST = $(man_MANS) abrt-handle-upload

if HAVE_SYSTEMD
dist_systemdsystemunit_DATA = \
    abrtd.service
endif

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(VAR_RUN)'
