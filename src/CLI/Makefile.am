bin_PROGRAMS = abrt-cli

abrt_cli_SOURCES = \
    CLI.cpp \
    ABRTSocket.h ABRTSocket.cpp

abrt_cli_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/Utils \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(ENABLE_SOCKET_OR_DBUS) \
    $(DBUS_CFLAGS) \
    -D_GNU_SOURCE
#    $(GTK_CFLAGS)

abrt_cli_LDADD = \
    ../../lib/Utils/libABRTUtils.la

man_MANS = abrt-cli.1
EXTRA_DIST = $(man_MANS)

completiondir = $(sysconfdir)/bash_completion.d
completion_DATA = abrt-cli.bash

