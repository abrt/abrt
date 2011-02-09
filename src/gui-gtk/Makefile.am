bin_PROGRAMS = abrt-gtk

abrt_gtk_SOURCES = \
    list_dir.c \
    list_dir.h \
    abrt-gtk.h \
    abrt-gtk.c \
    main.c
abrt_gtk_CFLAGS = \
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
    -D_GNU_SOURCE \
    -Wall -Werror
#    $(DBUS_CFLAGS)
#    -I/usr/include/glib-2.0
#    -I/usr/lib/glib-2.0/include
#    $(LIBNOTIFY_CFLAGS)
#    $(DBUS_GLIB_CFLAGS)
abrt_gtk_LDADD = \
    ../lib/libreport.la \
    ../lib/libabrt_dbus.la \
    -lglib-2.0 \
    -lgthread-2.0 \
    $(GTK_LIBS)
#    $(DBUS_LIBS)
#    $(LIBNOTIFY_LIBS)

#test_report_SOURCES = \
#    test_report.c
#test_report_CPPFLAGS = \
#    -I$(srcdir)/../include/report -I$(srcdir)/../include \
#    $(GLIB_CFLAGS) \
#    -D_GNU_SOURCE \
#    -Wall -Werror
#test_report_LDADD = \
#    ../lib/libreport.la

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

@INTLTOOL_DESKTOP_RULE@
