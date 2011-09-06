-include ../../config.mak

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
    CommLayerServerDBus.h CommLayerServerDBus.c \
    comm_layer_inner.h comm_layer_inner.c \
    abrtd.c
abrtd_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings
abrtd_LDADD = \
    $(DBUS_LIBS) \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS) \
    -labrt_dbus

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

abrt_handle_event_SOURCES = \
    abrt-handle-event.c
abrt_handle_event_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_handle_event_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

abrt_action_save_package_data_SOURCES = \
    rpm.h rpm.c \
    abrt-action-save-package-data.c
abrt_action_save_package_data_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
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

daemonconfdir = $(CONF_DIR)
dist_daemonconf_DATA = \
    abrt.conf \
    abrt-action-save-package-data.conf \
    gpg_keys

comredhatabrtservicedir = ${datadir}/dbus-1/system-services
dist_comredhatabrtservice_DATA = com.redhat.abrt.service

MAN_TXT = \
    abrt-handle-upload.txt \
    abrt-action-save-package-data.conf.txt \
    abrt.conf.txt \
    abrtd.txt \
    abrt-server.txt

# Manual pages are generated from .txt via Docbook
man1_MANS = abrt-handle-upload.1 abrt-server.1
man5_MANS = abrt.conf.5 abrt-action-save-package-data.conf.5
man8_MANS = abrtd.8

%.1 %.5 %.8: %.xml
	$(XMLTO_SILENT) xmlto man $< 2>&1 | sed '/Note/d'

%.xml: %.txt ../../asciidoc.conf
	$(ASCIIDOC_SILENT) asciidoc --backend=docbook --doctype=manpage --conf-file ../../asciidoc.conf -aabrt_version=$(PACKAGE_VERSION) -o $@ $<

EXTRA_DIST = $(MAN_TXT) abrt-handle-upload

CLEANFILES = $(man1_MANS) $(man5_MANS) $(man8_MANS)

if HAVE_SYSTEMD
dist_systemdsystemunit_DATA = \
    abrtd.service
endif

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(VAR_RUN)'
