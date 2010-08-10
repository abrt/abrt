bin_PROGRAMS = abrt-cli

abrt_cli_SOURCES = \
    CLI.cpp \
    run-command.h run-command.cpp \
    report.h report.cpp \
    dbus.h dbus.cpp

abrt_cli_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(ENABLE_SOCKET_OR_DBUS) \
    $(DBUS_CFLAGS) \
    -D_GNU_SOURCE
#    $(GTK_CFLAGS)

abrt_cli_LDADD = \
    ../../lib/utils/libABRTUtils.la \
    ../../lib/utils/libABRTdUtils.la

man_MANS = abrt-cli.1
EXTRA_DIST = $(man_MANS)

completiondir = $(sysconfdir)/bash_completion.d
dist_completion_DATA = abrt-cli.bash

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
