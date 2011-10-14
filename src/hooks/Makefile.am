-include ../../config.mak

pluginsconfdir = $(PLUGINS_CONF_DIR)

dist_pluginsconf_DATA = \
    CCpp.conf

sbin_SCRIPTS = \
    abrt-install-ccpp-hook \
    abrt-harvest-vmcore

libexec_PROGRAMS = abrt-hook-ccpp

# abrt-hook-ccpp
abrt_hook_ccpp_SOURCES = \
    abrt-hook-ccpp.c
abrt_hook_ccpp_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -Wall -Wwrite-strings \
    -D_GNU_SOURCE
abrt_hook_ccpp_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

MAN_TXT = \
    abrt-install-ccpp-hook.txt

# Manual pages are generated from .txt via Docbook
man1_MANS = abrt-install-ccpp-hook.1

%.1 %.5 %.8: %.xml
	$(XMLTO_SILENT) xmlto man $< 2>&1 | sed '/Note/d'

%.xml: %.txt ../../asciidoc.conf
	$(ASCIIDOC_SILENT) asciidoc --backend=docbook --doctype=manpage --conf-file ../../asciidoc.conf -aabrt_version=$(PACKAGE_VERSION) -o $@ $<

python_PYTHON = abrt.pth abrt_exception_handler.py
EXTRA_DIST = abrt_exception_handler.py.in abrt-install-ccpp-hook abrt-harvest-vmcore $(MAN_TXT)

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc)) $(man1_MANS)

# Must be synchronized with another sed call below.
abrt_exception_handler.py:
	sed s,\@VAR_RUN\@,\"$(VAR_RUN)\",g abrt_exception_handler.py.in > abrt_exception_handler.py

# RPM fix: we need to regenerate abrt_exception_handler.py, because it has the default ddir
install-data-local:
	sed s,\@VAR_RUN\@,\"$(VAR_RUN)\",g abrt_exception_handler.py.in > abrt_exception_handler.py
