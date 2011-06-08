-include ../../config.mak

bin_PROGRAMS = \
    abrt-cli

abrt_cli_SOURCES = \
    cli.c \
    run-command.h run-command.c \
    report.h report.c
abrt_cli_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(DBUS_CFLAGS) $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    $(LIBREPORT_CFLAGS) \
    -Wall -Wwrite-strings -Werror
#    $(GTK_CFLAGS)
abrt_cli_LDADD = \
    $(LIBREPORT_LIBS) -labrt_dbus \
    $(GLIB_LIBS)

MAN_TXT = \
    abrt-cli.txt

# Manual pages are generated from .txt via Docbook
man1_MANS = ${MAN_TXT:%.txt=%.1}

%.1 %.5 %.7: %.xml
	$(XMLTO_SILENT) xmlto man $< 2>&1 | sed '/Note/d'

%.xml: %.txt ../../asciidoc.conf
	$(ASCIIDOC_SILENT) asciidoc --backend=docbook --doctype=manpage --conf-file ../../asciidoc.conf -aabrt_version=$(PACKAGE_VERSION) -o $@ $<

CLEANFILES = $(man1_MANS)

PYTHON_FILES = \
    abrt-action-install-debuginfo \
    abrt-action-list-dsos.py \
    abrt-action-analyze-core.py

completiondir = $(sysconfdir)/bash_completion.d
dist_completion_DATA = abrt-cli.bash

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

EXTRA_DIST = $(MAN_TXT)

