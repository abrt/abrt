-include ../../config.mak

bin_PROGRAMS = abrt-cli

CLI_C =
CLI_C += abrt-cli.c

BUILTIN_C =
BUILTIN_C += list.c
BUILTIN_C += rm.c
BUILTIN_C += report.c

abrt_cli_SOURCES = $(CLI_C) $(BUILTIN_C) builtin-cmd.h

abrt_cli_CFLAGS = \
	-I$(srcdir)/../include \
	-I$(srcdir)/../lib \
	-DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
	$(LIBREPORT_CFLAGS) \
	-Wwrite-strings -Werror

abrt_cli_LDADD = \
    $(LIBREPORT_LIBS)

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
