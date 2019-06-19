bin_PROGRAMS = abrt-applet
#test-report

abrt_applet_SOURCES =          \
    abrt-applet-application.c  \
    abrt-applet-application.h  \
    abrt-applet-main.c         \
    abrt-applet-problem-info.c \
    abrt-applet-problem-info.h
abrt_applet_CPPFLAGS =              \
    -DGNU_SOURCE                    \
    -DLIBEXEC_DIR=\"$(libexecdir)\" \
    -DLOCALEDIR=\"$(localedir)\"    \
    -I$(srcdir)/../include          \
    $(GLIB_CFLAGS)                  \
    $(GTK_CFLAGS)                   \
    $(LIBREPORT_GTK_CFLAGS)         \
    $(POLKIT_CFLAGS)
abrt_applet_LDADD =       \
    ../lib/libabrt.la     \
    $(GLIB_LIBS)          \
    $(GTK_LIBS)           \
    $(LIBNOTIFY_LIBS)     \
    $(LIBREPORT_GTK_LIBS) \
    $(POLKIT_LIBS)

@INTLTOOL_DESKTOP_RULE@

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = org.freedesktop.problems.applet.desktop.in

autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

dbusservicedir = $(datadir)/dbus-1/services
dbusservice_in_files = org.freedesktop.problems.applet.service.in

dbusservice_DATA = org.freedesktop.problems.applet.service

org.freedesktop.problems.applet.service: org.freedesktop.problems.applet.service.in Makefile
	$(AM_V_GEN) sed -e "s|\@bindir\@|$(bindir)|" $< > $@

CLEANFILES = $(autostart_in_files:.desktop.in=.desktop)

EXTRA_DIST =              \
    $(autostart_in_files) \
    $(dbusservice_in_files)
