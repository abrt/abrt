bin_PROGRAMS = abrt-applet

abrt_applet_SOURCES = \
    Applet.cpp \
    CCApplet.h CCApplet.cpp
abrt_applet_CPPFLAGS = \
    -Wall -Werror \
    -I../Daemon/ \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/Utils \
    -I/usr/include/glib-2.0 \
    -I/usr/lib/glib-2.0/include \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DBIN_DIR=\"$(bindir)\" \
    $(DBUS_GLIB_CFLAGS) \
    $(GTK_CFLAGS) \
    $(DBUSCPP_CFLAGS) \
    $(LIBNOTIFY_CFLAGS)
abrt_applet_LDADD = \
    ../../lib/Utils/libABRTUtils.la \
    -lglib-2.0 -lgthread-2.0 \
    $(DL_LIBS) $(GTK_LIBS) \
    $(DBUSCPP_LIBS) $(LIBNOTIFY_LIBS)

EXTRA_DIST = abrt-applet.desktop popup.GtkBuilder

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = abrt-applet.desktop
