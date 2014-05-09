eventsconfdir = $(EVENTS_CONF_DIR)

dist_eventsconf_DATA = \
    abrt_event.conf

bin_SCRIPTS = \
    abrt-handle-upload

bin_PROGRAMS = \
    abrt-action-save-package-data

sbin_PROGRAMS = \
    abrtd \
    abrt-server \
    abrt-dbus \
    abrt-auto-reporting

libexec_PROGRAMS = abrt-handle-event

# This is a daemon, building with full relro and PIE
# for increased security.
abrtd_SOURCES = \
    abrtd.c
abrtd_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings \
    -Werror \
    -fPIE
abrtd_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)
abrtd_LDFLAGS = \
    -Wl,-z,relro -Wl,-z,now \
    -pie

abrt_server_SOURCES = \
    abrt-server.c
abrt_server_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_server_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

abrt_dbus_SOURCES = \
    abrt-dbus.c
abrt_dbus_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings \
    -Werror
abrt_dbus_LDADD = \
    $(DBUS_LIBS) \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS) \
    -labrt_dbus

abrt_handle_event_SOURCES = \
    abrt-handle-event.c
abrt_handle_event_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(SATYR_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_handle_event_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS) \
    $(SATYR_LIBS)

abrt_action_save_package_data_SOURCES = \
    rpm.h rpm.c \
    abrt-action-save-package-data.c
abrt_action_save_package_data_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_save_package_data_LDADD = \
    $(RPM_LIBS) \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

dbusabrtconfdir = ${sysconfdir}/dbus-1/system.d/
dist_dbusabrtconf_DATA = dbus-abrt.conf

abrt_auto_reporting_SOURCES = \
    abrt-auto-reporting.c
abrt_auto_reporting_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
abrt_auto_reporting_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

daemonconfdir = $(CONF_DIR)
dist_daemonconf_DATA = \
    abrt.conf \
    abrt-action-save-package-data.conf \
    gpg_keys

defaultdaemonconfdir = $(DEFAULT_CONF_DIR)
dist_defaultdaemonconf_DATA = \
    abrt.conf \
    abrt-action-save-package-data.conf

comredhatabrtservicedir = ${datadir}/dbus-1/system-services
dist_comredhatabrtservice_DATA = com.redhat.abrt.service
EXTRA_DIST = abrt-handle-upload

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(VAR_RUN)'
