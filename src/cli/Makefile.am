bin_PROGRAMS = abrt-cli

CLI_C =
CLI_C += abrt-cli.c
CLI_C += abrt-cli-core.c

BUILTIN_C =
BUILTIN_C += list.c
BUILTIN_C += rm.c
BUILTIN_C += report.c
BUILTIN_C += status.c

abrt_cli_SOURCES = $(CLI_C) $(BUILTIN_C) builtin-cmd.h abrt-cli-core.h

abrt_cli_CFLAGS = \
	-I$(srcdir)/../include \
	-I$(srcdir)/../lib \
	-DBIN_DIR=\"$(bindir)\" \
	$(LIBREPORT_CFLAGS) \
	-Wwrite-strings -Werror

abrt_cli_LDADD = \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

profileconfigdir = $(sysconfdir)/profile.d
dist_profileconfig_DATA = \
    abrt-console-notification.sh

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

install-exec-hook:
	ln -sf $(bindir)/consolehelper $(DESTDIR)$(bindir)/abrt-cli-root

install-data-hook:
	$(MKDIR_P) $(DESTDIR)$(sysconfdir)/pam.d $(DESTDIR)$(sysconfdir)/security/console.apps
	$(INSTALL_DATA) abrt-cli-root.pam $(DESTDIR)$(sysconfdir)/pam.d/abrt-cli-root
	$(INSTALL_DATA) abrt-cli-root.console $(DESTDIR)$(sysconfdir)/security/console.apps/abrt-cli-root

EXTRA_DIST = abrt-cli-root.console abrt-cli-root.pam
