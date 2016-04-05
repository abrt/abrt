-include ../../config.mak

bin_SCRIPTS = \
    abrt-action-install-debuginfo \
    abrt-action-analyze-core \
    abrt-action-analyze-vulnerability \
    abrt-action-list-dsos \
    abrt-action-perform-ccpp-analysis \
    abrt-action-analyze-ccpp-local \
    abrt-action-notify

if BUILD_BODHI
bin_SCRIPTS += \
    abrt-action-find-bodhi-update
endif

bin_PROGRAMS = \
    abrt-watch-log \
    abrt-dump-oops \
    abrt-dump-journal-core \
    abrt-dump-journal-oops \
    abrt-dump-xorg \
    abrt-dump-journal-xorg \
    abrt-action-analyze-c \
    abrt-action-analyze-python \
    abrt-action-analyze-oops \
    abrt-action-analyze-xorg \
    abrt-action-trim-files \
    abrt-action-generate-backtrace \
    abrt-action-generate-core-backtrace \
    abrt-action-analyze-backtrace \
    abrt-retrace-client

if BUILD_BODHI
bin_PROGRAMS += \
    abrt-bodhi
endif

libexec_PROGRAMS = \
    abrt-action-install-debuginfo-to-abrt-cache

libexec_SCRIPTS = \
    abrt-action-generate-machine-id \
    abrt-action-ureport \
    abrt-gdb-exploitable

#dist_pluginsconf_DATA = Python.conf

eventsdir = $(EVENTS_DIR)

dist_events_DATA = \
    analyze_CCpp.xml \
    analyze_LocalGDB.xml \
    analyze_RetraceServer.xml \
    collect_xsession_errors.xml \
    collect_GConf.xml \
    collect_vimrc_user.xml \
    collect_vimrc_system.xml \
    post_report.xml

@INTLTOOL_XML_RULE@

eventsconfdir = $(EVENTS_CONF_DIR)

dist_eventsconf_DATA = \
    ccpp_event.conf \
    ccpp_retrace_event.conf \
    koops_event.conf \
    xorg_event.conf \
    python_event.conf \
    python3_event.conf \
    smart_event.conf \
    gconf_event.conf \
    vimrc_event.conf


PYTHON_FILES = \
    abrt-action-install-debuginfo.in \
    abrt-action-list-dsos \
    abrt-action-analyze-core \
    abrt-action-analyze-vulnerability \
    abrt-action-check-oops-for-alt-component.in \
    abrt-action-check-oops-for-hw-error.in \
    abrt-action-perform-ccpp-analysis.in \
    abrt-action-notify

if BUILD_BODHI
PYTHON_FILES += \
    abrt-action-find-bodhi-update
endif

EXTRA_DIST = \
    $(PYTHON_FILES) \
    collect_xsession_errors.xml.in \
    collect_GConf.xml.in \
    collect_vimrc_user.xml.in \
    collect_vimrc_system.xml.in \
    analyze_CCpp.xml.in \
    analyze_LocalGDB.xml.in \
    analyze_RetraceServer.xml.in \
    abrt-action-analyze-core.in \
    abrt-action-generate-machine-id \
    abrt-action-ureport \
    abrt-gdb-exploitable \
    https-utils.h \
    oops-utils.h \
    xorg-utils.h \
    abrt-journal.h \
    post_report.xml.in \
    abrt-action-analyze-ccpp-local.in

if BUILD_ADDON_VMCORE
bin_SCRIPTS += \
    abrt-action-analyze-vmcore \
    abrt-action-check-oops-for-alt-component \
    abrt-action-check-oops-for-hw-error

dist_events_DATA += \
    analyze_VMcore.xml

dist_eventsconf_DATA += \
    vmcore_event.conf

PYTHON_FILES += \
    abrt-action-analyze-vmcore.in

EXTRA_DIST += \
    analyze_VMcore.xml.in \
    abrt-action-analyze-vmcore \
    abrt-action-check-oops-for-alt-component \
    abrt-action-check-oops-for-hw-error
endif

confdir = $(PLUGINS_CONF_DIR)
dist_conf_DATA = \
    xorg.conf
defaultconfdir = $(DEFAULT_PLUGINS_CONF_DIR)
dist_defaultconf_DATA = $(dist_conf_DATA)

abrt_watch_log_SOURCES = \
    abrt-watch-log.c
abrt_watch_log_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
abrt_watch_log_LDADD = \
    $(GLIB_LIBS) \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_dump_oops_SOURCES = \
    oops-utils.c \
    abrt-dump-oops.c
abrt_dump_oops_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    -D_GNU_SOURCE
abrt_dump_oops_LDADD = \
    $(GLIB_LIBS) \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

noinst_LIBRARIES = libabrt-journal.a
libabrt_journal_a_SOURCES = \
    abrt-journal.c \
    abrt-journal.h
libabrt_journal_a_CFLAGS = \
    -I$(srcdir)/../include \
    $(SYSTEMD_JOURNAL_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE

noinst_LIBRARIES += libxorg-utils.a
libxorg_utils_a_SOURCES = \
    xorg-utils.c \
    xorg-utils.h
libxorg_utils_a_CFLAGS = \
    -I$(srcdir)/../include \
    $(SYSTEMD_JOURNAL_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(GLIB_CFLAGS) \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    -D_GNU_SOURCE

abrt_dump_journal_oops_SOURCES = \
    oops-utils.c \
    abrt-dump-journal-oops.c
abrt_dump_journal_oops_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    -DVAR_STATE=\"$(VAR_STATE)\" \
    -D_GNU_SOURCE
abrt_dump_journal_oops_LDADD = \
    libabrt-journal.a \
    $(GLIB_LIBS) \
    $(LIBREPORT_LIBS) \
    $(SYSTEMD_JOURNAL_LIBS) \
    ../lib/libabrt.la

abrt_dump_journal_xorg_SOURCES = \
    abrt-dump-journal-xorg.c
abrt_dump_journal_xorg_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    -DVAR_STATE=\"$(VAR_STATE)\" \
    -D_GNU_SOURCE
abrt_dump_journal_xorg_LDADD = \
    libabrt-journal.a \
    libxorg-utils.a \
    $(GLIB_LIBS) \
    $(LIBREPORT_LIBS) \
    $(SYSTEMD_JOURNAL_LIBS) \
    ../lib/libabrt.la

abrt_dump_xorg_SOURCES = \
    abrt-dump-xorg.c
abrt_dump_xorg_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    -D_GNU_SOURCE
abrt_dump_xorg_LDADD = \
    libxorg-utils.a \
    $(GLIB_LIBS) \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_dump_journal_core_SOURCES = \
    abrt-dump-journal-core.c
abrt_dump_journal_core_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    -DVAR_STATE=\"$(VAR_STATE)\" \
    -D_GNU_SOURCE
abrt_dump_journal_core_LDADD = \
    libabrt-journal.a \
    $(GLIB_LIBS) \
    $(LIBREPORT_LIBS) \
    $(SYSTEMD_JOURNAL_LIBS) \
    ../lib/libabrt.la

abrt_action_analyze_c_SOURCES = \
    abrt-action-analyze-c.c
abrt_action_analyze_c_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
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
    -D_GNU_SOURCE
abrt_action_analyze_python_LDADD = \
    $(LIBREPORT_LIBS)

abrt_action_analyze_oops_SOURCES = \
    abrt-action-analyze-oops.c
abrt_action_analyze_oops_CPPFLAGS = \
    -I$(srcdir)/../libreport/src/include/ -I$(srcdir)/../include \
    -I$(srcdir)/../libreport/src/lib \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
abrt_action_analyze_oops_LDADD = \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_action_analyze_xorg_SOURCES = \
    abrt-action-analyze-xorg.c
abrt_action_analyze_xorg_CPPFLAGS = \
    -I$(srcdir)/../libreport/src/include/ -I$(srcdir)/../include \
    -I$(srcdir)/../libreport/src/lib \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
abrt_action_analyze_xorg_LDADD = \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la

abrt_action_trim_files_SOURCES = \
    abrt-action-trim-files.c
abrt_action_trim_files_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
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
    -D_GNU_SOURCE
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
    -D_GNU_SOURCE
abrt_action_generate_core_backtrace_LDADD = \
    $(LIBREPORT_LIBS) \
    $(SATYR_LIBS) \
    ../lib/libabrt.la

abrt_action_analyze_backtrace_SOURCES = \
    abrt-action-analyze-backtrace.c
abrt_action_analyze_backtrace_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(SATYR_CFLAGS) \
    -D_GNU_SOURCE
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
    -DBIN_DIR=\"$(bindir)\" \
    -DSBIN_DIR=\"$(sbindir)\" \
    -DLARGE_DATA_TMP_DIR=\"$(LARGE_DATA_TMP_DIR)\" \
    $(LIBREPORT_CFLAGS) \
    -Wall -Wwrite-strings \
    -fPIE
 abrt_action_install_debuginfo_to_abrt_cache_LDADD = \
    $(LIBREPORT_LIBS) \
    ../lib/libabrt.la
 abrt_action_install_debuginfo_to_abrt_cache_LDFLAGS = \
    -Wl,-z,relro -Wl,-z,now \
    -pie

abrt_retrace_client_SOURCES = \
    abrt-retrace-client.c \
    https-utils.c
 abrt_retrace_client_CFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
     $(NSS_CFLAGS) \
     $(GLIB_CFLAGS) \
     -D_GNU_SOURCE \
     -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
     -DLARGE_DATA_TMP_DIR=\"$(LARGE_DATA_TMP_DIR)\" \
     $(LIBREPORT_CFLAGS)
 abrt_retrace_client_LDADD = \
     $(LIBREPORT_LIBS) \
     $(SATYR_LIBS) \
     $(NSS_LIBS)

if BUILD_BODHI
abrt_bodhi_SOURCES = \
    bodhi.c
 abrt_bodhi_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(LIBREPORT_WEB_CFLAGS) \
    $(JSON_C_CFLAGS) \
    $(RPM_CFLAGS) \
    $(HAWKEY_CFLAGS) \
    -D_GNU_SOURCE
 abrt_bodhi_LDADD = \
    $(JSON_C_LIBS) \
    $(RPM_LIBS) \
    $(HAWKEY_LIBS) \
    $(LIBREPORT_LIBS) \
    $(LIBREPORT_WEB_LIBS)
endif

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

DISTCLEANFILES = abrt-action-analyze-ccpp-local abrt-action-analyze-core

abrt-action-perform-ccpp-analysis: abrt-action-perform-ccpp-analysis.in
	sed -e s,\@libexecdir\@,$(libexecdir),g \
        $< >$@

abrt-action-analyze-ccpp-local: abrt-action-analyze-ccpp-local.in
	sed -e s,\@LIBEXEC_DIR\@,$(libexecdir),g \
        $< >$@

abrt-action-analyze-core: abrt-action-analyze-core.in
	sed -e s,\@localedir\@,$(localedir),g \
        -e s,\@PACKAGE\@,$(PACKAGE),g \
        $< >$@
