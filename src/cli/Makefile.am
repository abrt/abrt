bin_PROGRAMS = \
    abrt-handle-crashdump \
    abrt-cli

abrt_cli_SOURCES = \
    cli.c \
    run-command.h run-command.c \
    report.h report.c
abrt_cli_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(DBUS_CFLAGS) $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
#    $(GTK_CFLAGS)
abrt_cli_LDADD = \
    ../lib/libreport.la \
    ../lib/libabrt_dbus.la \
    $(GLIB_LIBS)

abrt_handle_crashdump_SOURCES = \
    abrt-handle-crashdump.c
abrt_handle_crashdump_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DLIBEXEC_DIR=\"$(LIBEXEC_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_handle_crashdump_LDADD = \
    ../lib/libreport.la

man_MANS = abrt-cli.1
EXTRA_DIST = $(man_MANS)

completiondir = $(sysconfdir)/bash_completion.d
dist_completion_DATA = abrt-cli.bash

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
