eventsconfdir = $(EVENTS_CONF_DIR)

dist_eventsconf_DATA = \
    abrt_event.conf

bin_SCRIPTS = \
    abrt-handle-upload

bin_PROGRAMS = \
    abrt-action-save-package-data

sbin_PROGRAMS = \
    abrtd \
    abrt-server

libexec_PROGRAMS = abrt-handle-event

abrtd_SOURCES = \
    abrtd.c
abrtd_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DLIBEXEC_DIR=\"$(libexecdir)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -fPIE -Wl,-z,relro,now
abrtd_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

abrt_server_SOURCES = \
    abrt-server.c
abrt_server_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    -D_GNU_SOURCE
abrt_server_LDADD = \
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

daemonconfdir = $(CONF_DIR)
dist_daemonconf_DATA = \
    abrt.conf \
    abrt-action-save-package-data.conf \
    gpg_keys

EXTRA_DIST = abrt-handle-upload

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(VAR_RUN)'
