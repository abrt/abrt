-include ../../config.mak

pluginslibdir = $(PLUGINS_LIB_DIR)

bin_SCRIPTS = \
    abrt-action-install-debuginfo \
    abrt-action-analyze-core \
    abrt-action-list-dsos

bin_PROGRAMS = \
    abrt-dump-oops \
    abrt-action-analyze-c \
    abrt-action-analyze-python \
    abrt-action-analyze-oops \
    abrt-action-trim-files \
    abrt-action-generate-backtrace \
    abrt-action-analyze-backtrace \
    abrt-action-install-debuginfo-to-abrt-cache \
    abrt-retrace-client

pluginsconfdir = $(PLUGINS_CONF_DIR)

#dist_pluginsconf_DATA = Python.conf

eventsdir = $(EVENTS_DIR)

dist_events_DATA = \
    analyze_LocalGDB.xml \
    analyze_RetraceServer.xml \
    analyze_xsession_errors.xml \
    analyze_Smolt.xml

@INTLTOOL_XML_RULE@

eventsconfdir = $(EVENTS_CONF_DIR)

dist_eventsconf_DATA = \
    ccpp_event.conf \
    koops_event.conf \
    python_event.conf \
    smart_event.conf \
    smolt_event.conf

man_MANS = \
    abrt-plugins.7

MAN_TXT = \
    abrt-action-trim-files.txt \
    abrt-action-generate-backtrace.txt \
    abrt-action-analyze-backtrace.txt \
    abrt-action-analyze-oops.txt \
    abrt-action-list-dsos.txt \
    abrt-retrace-client.txt

# Manual pages are generated from .txt via Docbook
man1_MANS = ${MAN_TXT:%.txt=%.1}

%.1 %.5 %.7: %.xml
	$(XMLTO_SILENT) xmlto man $< 2>&1 | sed '/Note/d'

%.xml: %.txt ../../asciidoc.conf
	$(ASCIIDOC_SILENT) asciidoc --backend=docbook --doctype=manpage --conf-file ../../asciidoc.conf -aabrt_version=$(PACKAGE_VERSION) -o $@ $<


CLEANFILES = $(man1_MANS)

PYTHON_FILES = \
    abrt-action-install-debuginfo \
    abrt-action-list-dsos \
    abrt-action-analyze-core

EXTRA_DIST = \
    $(man_MANS) \
    $(MAN_TXT) \
    $(PYTHON_FILES) \
    $(man1_MANS) \
    analyze_Smolt.xml.in \
    analyze_xsession_errors.xml.in \
    analyze_LocalGDB.xml.in \
    analyze_RetraceServer.xml.in

$(DESTDIR)/$(DEBUG_INFO_DIR):
	$(mkdir_p) '$@'

abrt_dump_oops_SOURCES = \
    abrt-dump-oops.c
abrt_dump_oops_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_dump_oops_LDADD = \
    $(GLIB_LIBS) \
    $(LIBREPORT_LIBS)

abrt_action_analyze_c_SOURCES = \
    abrt-action-analyze-c.c
abrt_action_analyze_c_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_analyze_c_LDADD = \
    $(LIBREPORT_LIBS)

abrt_action_analyze_python_SOURCES = \
    abrt-action-analyze-python.c
abrt_action_analyze_python_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_analyze_python_LDADD = \
    $(LIBREPORT_LIBS)

abrt_action_analyze_oops_SOURCES = \
    abrt-action-analyze-oops.c
abrt_action_analyze_oops_CPPFLAGS = \
    -I$(srcdir)/../libreport/src/include/ -I$(srcdir)/../include \
    -I$(srcdir)/../libreport/src/lib \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_analyze_oops_LDADD = \
    $(LIBREPORT_LIBS)

abrt_action_trim_files_SOURCES = \
    abrt-action-trim-files.c
abrt_action_trim_files_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_trim_files_LDADD = \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_action_generate_backtrace_SOURCES = \
    abrt-action-generate-backtrace.c
abrt_action_generate_backtrace_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_generate_backtrace_LDADD = \
    $(LIBREPORT_LIBS)

abrt_action_analyze_backtrace_SOURCES = \
    abrt-action-analyze-backtrace.c
abrt_action_analyze_backtrace_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_analyze_backtrace_LDADD = \
    $(LIBREPORT_LIBS) \
    $(BTPARSER_LIBS)

abrt_action_install_debuginfo_to_abrt_cache_SOURCES = \
    abrt-action-install-debuginfo-to-abrt-cache.c
abrt_action_install_debuginfo_to_abrt_cache_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -D_GNU_SOURCE \
    $(LIBREPORT_CFLAGS) \
    -Wall -Wwrite-strings
abrt_action_install_debuginfo_to_abrt_cache_LDADD =

abrt_retrace_client_SOURCES = \
    abrt-retrace-client.c
 abrt_retrace_client_CFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
     $(NSS_CFLAGS) \
     $(GLIB_CFLAGS) \
     -D_GNU_SOURCE \
     $(LIBREPORT_CFLAGS) \
     -Wall -Wwrite-strings -Werror
 abrt_retrace_client_LDADD = \
     $(LIBREPORT_LIBS) \
     $(BTPARSER_LIBS) \
     $(NSS_LIBS)

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

if HAVE_SYSTEMD
dist_systemdsystemunit_DATA = \
    abrt-oops.service
endif
