# libreportgtk - the stuff shared among gtk clients (abrt-gui, wizard)
libreport_gtk_includedir = \
    $(includedir)/libreport

libreport_gtk_include_HEADERS = \
    libreport-gtk.h

lib_LTLIBRARIES = \
    libreport-gtk.la

libreport_gtk_la_SOURCES = \
    event_config_dialog.c \
    abrt-keyring.c \
    autowrapped_label.c

libreport_gtk_la_CPPFLAGS = \
    -Wall -Wwrite-strings -Werror \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GTK_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(GNOME_KEYRING_CFLAGS) \
    -D_GNU_SOURCE
libreport_gtk_la_LDFLAGS = \
    -version-info 0:1:0
libreport_gtk_la_LIBADD = \
    $(GTK_LIBS) \
    $(GLIB_LIBS) \
    $(GNOME_KEYRING_LIBS)

EXTRA_DIST = libreport-gtk.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libreport-gtk.pc
