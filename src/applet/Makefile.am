bin_PROGRAMS = abrt-applet
#test-report

abrt_applet_SOURCES = \
    applet.c
abrt_applet_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DLIBEXEC_DIR=\"$(libexecdir)\" \
    -DLIBREPORT_PLUGINS_CONF_DIR=\"$(LIBREPORT_PLUGINS_CONF_DIR)\" \
    -DLOCALEDIR=\"$(localedir)\" \
    $(GTK_CFLAGS) \
    $(GIO_UNIX_CFLAGS) \
    $(POLKIT_CFLAGS) \
    $(LIBREPORT_GTK_CFLAGS) \
    -D_GNU_SOURCE
abrt_applet_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_GTK_LIBS) \
    -labrt_dbus \
    -lglib-2.0 \
    -lgthread-2.0 \
    $(LIBNOTIFY_LIBS) \
    $(GTK_LIBS) \
    $(GIO_UNIX_LIBS) \
    $(POLKIT_LIBS)

@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = abrt-applet.desktop.in

autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

CLEANFILES = $(autostart_in_files:.desktop.in=.desktop)

EXTRA_DIST = $(autostart_in_files)
