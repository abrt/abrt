#bin_PROGRAMS = abrt-cli
#abrt_cli_SOURCES = ABRTSocket.cpp ABRTSocket.h CLI.cpp
#abrt_cli_CPPFLAGS = -I$(srcdir)/../../inc -DVAR_RUN=\"$(VAR_RUN)\"

bin_PROGRAMS = abrt-cli

abrt_cli_SOURCES = \
    CLI.cpp \
    ABRTSocket.h ABRTSocket.cpp
abrt_cli_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/Utils \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(ENABLE_SOCKET_OR_DBUS) \
    $(DBUS_GLIB_CFLAGS) \
    $(GTK_CFLAGS) \
    $(DBUSCPP_CFLAGS) \
    $(LIBNOTIFY_CFLAGS)
abrt_cli_LDADD = \
    ../../lib/Utils/libABRTUtils.la
