bin_PROGRAMS = abrt-applet

abrt_applet_SOURCES = \
    Applet.cpp \
    CCApplet.h CCApplet.cpp
abrt_applet_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/Utils \
    -I/usr/include/glib-2.0 \
    -I/usr/lib/glib-2.0/include \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DICON_DIR=\"${datadir}/abrt/icons/hicolor/48x48/status\" \
    $(GTK_CFLAGS) \
    $(DBUS_CFLAGS) \
    -D_GNU_SOURCE
#    $(LIBNOTIFY_CFLAGS)
#    $(DBUS_GLIB_CFLAGS)
abrt_applet_LDADD = \
    ../../lib/Utils/libABRTUtils.la \
    ../../lib/Utils/libABRTdUtils.la \
    -lglib-2.0 \
    -lgthread-2.0 \
    $(DBUS_LIBS) \
    $(LIBNOTIFY_LIBS)
#    ../../lib/Utils/libABRTUtils.la
#    $(DL_LIBS)
#    $(GTK_LIBS)

EXTRA_DIST = abrt-applet.desktop popup.GtkBuilder

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = abrt-applet.desktop
