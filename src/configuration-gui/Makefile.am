lib_LTLIBRARIES = libabrtconfigui.la

libabrtconfigui_la_SOURCES = \
	abrt-config-widget.c \
	abrt-config-widget.h

libabrtconfigui_la_CPPFLAGS = \
	-I$(srcdir)/../include \
	-I$(srcdir)/../lib \
	$(LIBREPORT_CFLAGS) \
	$(GTK_CFLAGS) \
	-DABRT_UI_DIR="\"$(uidir)\"" \
	-DLOCALEDIR="\"$(localedir)\""

libabrtconfigui_la_LDFLAGS = \
	-version-info 0:1:0

libabrtconfigui_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBREPORT_LIBS)

bin_PROGRAMS = system-config-abrt

system_config_abrt_CPPFLAGS = \
	-I$(srcdir)/../include \
	-I$(srcdir)/../lib \
	$(GTK_CFLAGS) \
	$(LIBREPORT_CFLAGS)

system_config_abrt_SOURCES = \
	system-config-abrt.c

system_config_abrt_LDADD = \
	../lib/libabrt.la \
	libabrtconfigui.la
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBREPORT_LIBS)

dist_ui_DATA = abrt-config-widget.ui
uidir = $(pkgdatadir)/ui

desktopdir = $(datadir)/applications
desktop_in_files = system-config-abrt.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

CLEANFILES = $(desktop_DATA)
EXTRA_DIST = $(desktop_in_files)
