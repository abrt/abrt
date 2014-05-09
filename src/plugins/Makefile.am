-include ../../config.mak

bin_SCRIPTS = \
    abrt-action-install-debuginfo \
    abrt-action-analyze-core \
    abrt-action-analyze-vmcore \
    abrt-action-list-dsos \
    abrt-action-analyze-ccpp-local \
    abrt-action-save-kernel-data \
    abrt-action-notify

bin_PROGRAMS = \
    abrt-dump-oops \
    abrt-action-analyze-c \
    abrt-action-analyze-python \
    abrt-action-analyze-oops \
    abrt-action-trim-files \
    abrt-action-generate-backtrace \
    abrt-action-generate-core-backtrace \
    abrt-action-analyze-backtrace \
    abrt-dedup-client

libexec_PROGRAMS = abrt-action-install-debuginfo-to-abrt-cache

#dist_pluginsconf_DATA = Python.conf

eventsdir = $(EVENTS_DIR)

dist_events_DATA = \
    analyze_LocalGDB.xml \
    analyze_VMcore.xml \
    collect_xsession_errors.xml \
    collect_GConf.xml \
    collect_vimrc_user.xml \
    collect_vimrc_system.xml

@INTLTOOL_XML_RULE@

eventsconfdir = $(EVENTS_CONF_DIR)

dist_eventsconf_DATA = \
    ccpp_event.conf \
    koops_event.conf \
    vmcore_event.conf \
    python_event.conf \
    smart_event.conf \
    gconf_event.conf \
    vimrc_event.conf


PYTHON_FILES = \
    abrt-action-install-debuginfo.in \
    abrt-action-list-dsos \
    abrt-action-analyze-core \
    abrt-action-analyze-vmcore.in \
    abrt-action-notify

EXTRA_DIST = \
    $(PYTHON_FILES) \
    collect_xsession_errors.xml.in \
    collect_GConf.xml.in \
    collect_vimrc_user.xml.in \
    collect_vimrc_system.xml.in \
    analyze_LocalGDB.xml.in \
    analyze_VMcore.xml.in \
    abrt-action-analyze-vmcore \
    https-utils.h \
    abrt-action-analyze-ccpp-local \
    abrt-action-save-kernel-data

abrt_dump_oops_SOURCES = \
    abrt-dump-oops.c
abrt_dump_oops_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_dump_oops_LDADD = \
    $(GLIB_LIBS) \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_action_analyze_c_SOURCES = \
    abrt-action-analyze-c.c
abrt_action_analyze_c_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_analyze_c_LDADD = \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_action_analyze_python_SOURCES = \
    abrt-action-analyze-python.c
abrt_action_analyze_python_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
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
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_analyze_oops_LDADD = \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_action_trim_files_SOURCES = \
    abrt-action-trim-files.c
abrt_action_trim_files_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
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
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_generate_backtrace_LDADD = \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_action_generate_core_backtrace_SOURCES = \
    abrt-action-generate-core-backtrace.c
abrt_action_generate_core_backtrace_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(SATYR_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_generate_core_backtrace_LDADD = \
    $(LIBREPORT_LIBS) \
    $(SATYR_LIBS) \
    ../lib/libabrt.la

if ENABLE_DISASSEMBLY
abrt_action_generate_core_backtrace_CPPFLAGS += -DENABLE_DISASSEMBLY
abrt_action_generate_core_backtrace_LDADD += \
    -lelf \
    -ldw \
    -ldl \
    -lbfd \
    -lopcodes
endif

abrt_action_analyze_backtrace_SOURCES = \
    abrt-action-analyze-backtrace.c
abrt_action_analyze_backtrace_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Wwrite-strings -Werror
abrt_action_analyze_backtrace_LDADD = \
    $(LIBREPORT_LIBS) \
    $(SATYR_LIBS)

# SUID application, building with full relro and PIE
# for increased security.
abrt_action_install_debuginfo_to_abrt_cache_SOURCES = \
    abrt-action-install-debuginfo-to-abrt-cache.c
abrt_action_install_debuginfo_to_abrt_cache_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -D_GNU_SOURCE \
    $(LIBREPORT_CFLAGS) \
    -Wall -Wwrite-strings \
    -fPIE
 abrt_action_install_debuginfo_to_abrt_cache_LDADD = \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la
 abrt_action_install_debuginfo_to_abrt_cache_LDFLAGS = \
    -Wl,-z,relro -Wl,-z,now \
    -pie

abrt_dedup_client_SOURCES = \
    abrt-dedup-client.c \
    https-utils.c
 abrt_dedup_client_CFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
     $(NSS_CFLAGS) \
     $(GLIB_CFLAGS) \
     -D_GNU_SOURCE \
     $(LIBREPORT_CFLAGS) \
     -Wall -Wwrite-strings -Werror
 abrt_dedup_client_LDADD = \
     $(LIBREPORT_LIBS) \
     $(SATYR_LIBS) \
     $(NSS_LIBS)

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
