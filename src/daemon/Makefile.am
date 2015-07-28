eventsconfdir = $(EVENTS_CONF_DIR)

dist_eventsconf_DATA = \
    abrt_event.conf

bin_SCRIPTS = \
    abrt-handle-upload

bin_PROGRAMS = \
    abrt-action-save-package-data \
    abrt-action-save-container-data

sbin_PROGRAMS = \
    abrtd \
    abrt-server \
    abrt-upload-watch \
    abrt-auto-reporting

libexec_PROGRAMS = abrt-handle-event

# This is a daemon, building with full relro and PIE
# for increased security.
abrtd_SOURCES = \
    abrtd.c \
    abrt-inotify.c \
    abrt-inotify.h
abrtd_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DLIBEXEC_DIR=\"$(libexecdir)\" \
    -DDEFAULT_DUMP_LOCATION_MODE=$(DEFAULT_DUMP_LOCATION_MODE) \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
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
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    -DLIBEXEC_DIR=\"$(libexecdir)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
abrt_server_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

abrt_upload_watch_SOURCES = \
    abrt-upload-watch.c \
    abrt-inotify.c \
    abrt-inotify.h
abrt_upload_watch_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    -DLIBEXEC_DIR=\"$(libexecdir)\" \
    $(GLIB_CFLAGS) \
    $(GIO_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
abrt_upload_watch_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)


abrt_handle_event_SOURCES = \
    abrt-handle-event.c
abrt_handle_event_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(SATYR_CFLAGS) \
    -D_GNU_SOURCE
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
    -D_GNU_SOURCE
abrt_action_save_package_data_LDADD = \
    $(RPM_LIBS) \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_action_save_container_data_SOURCES = \
    abrt-action-save-container-data.c
abrt_action_save_container_data_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(JSON_C_CFLAGS) \
    -D_GNU_SOURCE
abrt_action_save_container_data_LDADD = \
    $(LIBREPORT_LIBS) \
    $(JSON_C_LIBS)

abrt_auto_reporting_SOURCES = \
    abrt-auto-reporting.c
abrt_auto_reporting_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE

if AUTHENTICATED_AUTOREPORTING
abrt_auto_reporting_CPPFLAGS += -DAUTHENTICATED_AUTOREPORTING=1
endif

abrt_auto_reporting_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

daemonconfdir = $(CONF_DIR)
dist_daemonconf_DATA = \
    abrt.conf \
    abrt-action-save-package-data.conf \
    gpg_keys.conf
defaultdaemonconfdir = $(DEFAULT_CONF_DIR)
dist_defaultdaemonconf_DATA = $(dist_daemonconf_DATA)

dbusabrtconfdir = ${sysconfdir}/dbus-1/system.d/
dist_dbusabrtconf_DATA = org.freedesktop.problems.daemon.conf

EXTRA_DIST = abrt-handle-upload.in

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(VAR_RUN)'
