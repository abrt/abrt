sbin_PROGRAMS = \
    abrt-dbus

abrt_dbus_SOURCES = \
    abrt-dbus.c \
    abrt-polkit.c \
    abrt-polkit.h
abrt_dbus_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GIO_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(POLKIT_CFLAGS) \
    -D_GNU_SOURCE
abrt_dbus_LDADD = \
    $(GIO_LIBS) \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS) \
    $(POLKIT_LIBS) \
    -labrt_dbus

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

polkitconfdir = ${datadir}/polkit-1/actions
dist_polkitconf_DATA = abrt_polkit.policy

dbusabrtconfdir = ${sysconfdir}/dbus-1/system.d/
dist_dbusabrtconf_DATA = dbus-abrt.conf

orgfreedesktopproblemsservicedir = ${datadir}/dbus-1/system-services
dist_orgfreedesktopproblemsservice_DATA = org.freedesktop.problems.service
