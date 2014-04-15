libabrt_gui_includedir = \
	$(includedir)/abrt

libabrt_gui_include_HEADERS = \
	abrt-config-widget.h \
	system-config-abrt.h

lib_LTLIBRARIES = libabrt_gui.la

libabrt_gui_la_SOURCES = \
	abrt-config-widget.c \
	abrt-config-widget.h \
	system-config-abrt.c \
	system-config-abrt.h

# G_DEFINE_TYPE(...) macros result in
# typedef '_GStaticAssertCompileTimeAssertion_0' locally defined but not used
# warnings on some glib versions
# (observed on glib2-2.34.2 on F18).
# -Wno-error=unused-local-typedefs in CPPFLAGS suppresses this warning.
# Newer glib may have it fixed.

libabrt_gui_la_CPPFLAGS = \
	-I$(srcdir)/../include \
	-I$(srcdir)/../lib \
	-Wno-error=unused-local-typedefs \
	$(LIBREPORT_CFLAGS) \
	$(GTK_CFLAGS) \
	-DABRT_UI_DIR="\"$(uidir)\"" \
	-DLOCALEDIR="\"$(localedir)\""

libabrt_gui_la_LDFLAGS = \
	-version-info 0:1:0

libabrt_gui_la_LIBADD = \
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBREPORT_LIBS)

bin_PROGRAMS = system-config-abrt

system_config_abrt_CPPFLAGS = \
	-I$(srcdir)/../include \
	-I$(srcdir)/../lib \
	-Wno-error=unused-local-typedefs \
	$(GTK_CFLAGS) \
	$(LIBREPORT_CFLAGS)

system_config_abrt_SOURCES = \
	main.c

system_config_abrt_LDADD = \
	../lib/libabrt.la \
	libabrt_gui.la
	$(GLIB_LIBS) \
	$(GTK_LIBS) \
	$(LIBREPORT_LIBS)

dist_ui_DATA = abrt-config-widget.glade
uidir = $(pkgdatadir)/ui

EXTRA_DIST = abrt_gui.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abrt_gui.pc
