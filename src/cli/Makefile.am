bin_PROGRAMS = abrt-cli

abrt_cli_SOURCES = \
    CLI.cpp \
    run-command.h run-command.c \
    report.h report.cpp \
    dbus.h dbus.cpp
abrt_cli_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(ENABLE_SOCKET_OR_DBUS) \
    $(DBUS_CFLAGS) $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
#    $(GTK_CFLAGS)
abrt_cli_LDADD = \
    ../lib/libreport.la \
    ../lib/libabrt_dbus.la \
    $(GLIB_LIBS)

man_MANS = abrt-cli.1
EXTRA_DIST = $(man_MANS)

completiondir = $(sysconfdir)/bash_completion.d
dist_completion_DATA = abrt-cli.bash

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
