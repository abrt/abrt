bin_PROGRAMS = abrt-gui

abrt_gui_SOURCES = \
    main.c
abrt_gui_CFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -I$(srcdir)/../gtk-helpers \
    -DBIN_DIR=\"$(bindir)\" \
    -DICON_DIR=\"${datadir}/abrt/icons/hicolor/48x48/status\" \
    $(GTK_CFLAGS) \
    $(LIBREPORT_GTK_CFLAGS) \
    -D_GNU_SOURCE \
    $(GIO_CFLAGS) \
    -Wall -Wwrite-strings -Werror
#    -I/usr/include/glib-2.0
#    -I/usr/lib/glib-2.0/include
#    $(LIBNOTIFY_CFLAGS)
#    $(DBUS_GLIB_CFLAGS)
abrt_gui_LDADD = \
    -lglib-2.0 \
    -lgthread-2.0 \
    $(GIO_LIBS) \
    $(LIBREPORT_GTK_LIBS) \
    ../lib/libabrt.la \
    $(GTK_LIBS)
#    $(LIBNOTIFY_LIBS)

#test_report_SOURCES = \
#    test_report.c
#test_report_CPPFLAGS = \
#    -I$(srcdir)/../include/report -I$(srcdir)/../include \
#    $(GLIB_CFLAGS) \
#    -D_GNU_SOURCE \
#    -Wall -Wwrite-strings -Werror
#test_report_LDADD = \
#    ../lib/libreport.la

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = abrt.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = $(desktop_in_files)
