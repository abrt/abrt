bin_PROGRAMS = abrt-applet
#test-report

abrt_applet_SOURCES = \
    applet.c
abrt_applet_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DLIBEXEC_DIR=\"$(libexecdir)\" \
    -DICON_DIR=\"${datadir}/abrt/icons/hicolor/48x48/status\" \
    -DLIBREPORT_PLUGINS_CONF_DIR=\"$(LIBREPORT_PLUGINS_CONF_DIR)\" \
    $(GTK_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(LIBREPORT_GTK_CFLAGS) \
    -D_GNU_SOURCE
#    -I/usr/include/glib-2.0
#    -I/usr/lib/glib-2.0/include
#    $(LIBNOTIFY_CFLAGS)
#    $(DBUS_GLIB_CFLAGS)
abrt_applet_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_GTK_LIBS) \
    -labrt_dbus \
    -lglib-2.0 \
    -lgthread-2.0 \
    $(DBUS_LIBS) \
    $(LIBNOTIFY_LIBS) \
    $(GTK_LIBS)

#test_report_SOURCES = \
#    test_report.c
#test_report_CPPFLAGS = \
#    -I$(srcdir)/../include/report -I$(srcdir)/../include \
#    $(GLIB_CFLAGS) \
#    -D_GNU_SOURCE
#test_report_LDADD = \
#    ../lib/libreport.la

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = abrt-applet.desktop.in

autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

EXTRA_DIST = $(autostart_in_files)
