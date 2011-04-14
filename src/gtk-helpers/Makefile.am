# libreportgtk - the stuff shared among gtk clients (abrt-gui, wizard)
lib_LTLIBRARIES = \
    libreportgtk.la

libreportgtk_la_SOURCES = \
    libreport-gtk.h \
    event_config_dialog.c \
    abrt-keyring.c \
    autowrapped_label.c

libreportgtk_la_CPPFLAGS = \
    -Wall -Wwrite-strings -Werror \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GTK_CFLAGS) \
    $(GLIB_CFLAGS) \
    $(GNOME_KEYRING_CFLAGS) \
    -D_GNU_SOURCE
libreportgtk_la_LDFLAGS = \
    -version-info 0:1:0
libreportgtk_la_LIBADD = \
    $(GTK_LIBS) \
    $(GLIB_LIBS) \
    $(GNOME_KEYRING_LIBS)
