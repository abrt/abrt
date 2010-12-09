bin_PROGRAMS = abrt-applet
#test-report

abrt_applet_SOURCES = \
    Applet.cpp \
    applet_gtk.h applet_gtk.c
abrt_applet_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DICON_DIR=\"${datadir}/abrt/icons/hicolor/48x48/status\" \
    $(GTK_CFLAGS) \
    $(DBUS_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
#    -I/usr/include/glib-2.0
#    -I/usr/lib/glib-2.0/include
#    $(LIBNOTIFY_CFLAGS)
#    $(DBUS_GLIB_CFLAGS)
abrt_applet_LDADD = \
    ../lib/libabrt.la \
    ../lib/libabrt_dbus.la \
    -lglib-2.0 \
    -lgthread-2.0 \
    $(DBUS_LIBS) \
    $(LIBNOTIFY_LIBS) \
    $(GTK_LIBS)

#test_report_SOURCES = \
#    test_report.cpp
#test_report_CPPFLAGS = \
#    -Wall -Werror \
#    -I$(srcdir)/../include/report -I$(srcdir)/../include \
#    -D_GNU_SOURCE
#test_report_LDADD = \
#    ../lib/libabrt.la

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = abrt-applet.desktop.in

autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

EXTRA_DIST = $(autostart_in_files)
