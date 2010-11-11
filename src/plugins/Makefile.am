INC_PATH=$(srcdir)/../../inc
UTILS_PATH=$(srcdir)/../../lib/utils
AM_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH)
pluginslibdir=$(PLUGINS_LIB_DIR)
libexec_SCRIPTS = \
    abrt-action-install-debuginfo

pluginslib_LTLIBRARIES = \
    libCCpp.la \
    libMailx.la \
    libSQLite3.la \
    libKerneloopsScanner.la\
    libKerneloops.la \
    libSOSreport.la \
    libReportUploader.la \
    libPython.la \
    libFileTransfer.la

dist_pluginslib_DATA = \
    Logger.glade \
    Mailx.glade \
    Bugzilla.glade \
    RHTSupport.glade \
    ReportUploader.glade \
    KerneloopsReporter.glade

pluginsconfdir = $(PLUGINS_CONF_DIR)
dist_pluginsconf_DATA = \
    CCpp.conf \
    Mailx.conf \
    SQLite3.conf \
    Logger.conf \
    Kerneloops.conf \
    Bugzilla.conf \
    RHTSupport.conf \
    ReportUploader.conf \
    FileTransfer.conf \
    Python.conf \
    SOSreport.conf

man_MANS = \
    abrt-FileTransfer.7 \
    abrt-Bugzilla.7 \
    abrt-KerneloopsScanner.7 \
    abrt-KerneloopsReporter.7 \
    abrt-Logger.7 \
    abrt-Mailx.7 \
    abrt-plugins.7 \
    abrt-SQLite3.7 \
    abrt-ReportUploader.7

EXTRA_DIST = $(man_MANS) abrt-action-install-debuginfo

$(DESTDIR)/$(DEBUG_INFO_DIR):
	$(mkdir_p) '$@'

install-data-hook: $(DESTDIR)/$(DEBUG_INFO_DIR)
	sed 's: = /var/: = $(localstatedir)/:g' -i \
	    $(DESTDIR)$(sysconfdir)/abrt/plugins/SQLite3.conf \
	    $(DESTDIR)$(sysconfdir)/abrt/plugins/Logger.conf

# CCpp
libCCpp_la_SOURCES = CCpp.cpp CCpp.h
libCCpp_la_LDFLAGS = -avoid-version
libCCpp_la_CPPFLAGS = -Wall -Werror \
    -I$(INC_PATH) -I$(UTILS_PATH) \
    -DCCPP_HOOK_PATH=\"${libexecdir}/abrt-hook-ccpp\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"'
#    -DHOSTILE_KERNEL

# Kerneloops
libKerneloops_la_SOURCES = Kerneloops.cpp Kerneloops.h
libKerneloops_la_LDFLAGS = -avoid-version
libKerneloops_la_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH)

# KerneloopsScanner
libKerneloopsScanner_la_SOURCES = KerneloopsScanner.cpp KerneloopsScanner.h KerneloopsSysLog.cpp KerneloopsSysLog.h
libKerneloopsScanner_la_LDFLAGS = -avoid-version $(GLIB_LIBS)
libKerneloopsScanner_la_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH) -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" $(GLIB_CFLAGS)

# Mailx
libMailx_la_SOURCES = Mailx.cpp Mailx.h
libMailx_la_LDFLAGS = -avoid-version
libMailx_la_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH) -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" -DLOCALSTATEDIR='"$(localstatedir)"'

# SQLite3
libSQLite3_la_SOURCES = SQLite3.cpp SQLite3.h
libSQLite3_la_LDFLAGS = -avoid-version
libSQLite3_la_LIBADD = $(SQLITE3_LIBS) $(GLIB_LIBS)
libSQLite3_la_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH) $(SQLITE3_CFLAGS) -DLOCALSTATEDIR='"$(localstatedir)"' $(GLIB_CFLAGS)

# SOSreport
libSOSreport_la_SOURCES = SOSreport.cpp SOSreport.h
libSOSreport_la_LDFLAGS = -avoid-version

# ReportUploader
libReportUploader_la_SOURCES = ReportUploader.h ReportUploader.cpp
libReportUploader_la_LDFLAGS = -avoid-version
libReportUploader_la_LIBADD = $(CURL_LIBS)
libReportUploader_la_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH) $(CURL_CFLAGS) -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\"

# Python
libPython_la_SOURCES = Python.h Python.cpp
#libPython_la_LIBADD = $(NSS_LIBS)
libPython_la_LDFLAGS = -avoid-version
libPython_la_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH)

# FileTrasfer
libFileTransfer_la_SOURCES = FileTransfer.cpp FileTransfer.h
libFileTransfer_la_LDFLAGS = -avoid-version -ltar -lbz2 -lz
libFileTransfer_la_LIBADD = $(CURL_LIBS)
libFileTransfer_la_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH) $(CURL_CFLAGS) -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"

libexec_PROGRAMS = \
    abrt-action-analyze-c \
    abrt-action-analyze-python \
    abrt-action-analyze-oops \
    abrt-action-generate-backtrace \
    abrt-action-bugzilla \
    abrt-action-rhtsupport \
    abrt-action-kerneloops \
    abrt-action-print

abrt_action_analyze_c_SOURCES = \
    abrt-action-analyze-c.c
abrt_action_analyze_c_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_analyze_c_LDADD = \
    ../../lib/utils/libABRTUtils.la

abrt_action_analyze_python_SOURCES = \
    abrt-action-analyze-python.c
abrt_action_analyze_python_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_analyze_python_LDADD = \
    ../../lib/utils/libABRTUtils.la

abrt_action_analyze_oops_SOURCES = \
    abrt-action-analyze-oops.c
abrt_action_analyze_oops_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_analyze_oops_LDADD = \
    ../../lib/utils/libABRTUtils.la

abrt_action_generate_backtrace_SOURCES = \
    abrt-action-generate-backtrace.c
abrt_action_generate_backtrace_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_generate_backtrace_LDADD = \
    ../../lib/utils/libABRTUtils.la \
    ../btparser/libbtparser.la

abrt_action_bugzilla_SOURCES = \
    abrt-action-bugzilla.cpp
abrt_action_bugzilla_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_bugzilla_LDADD = \
    $(GLIB_LIBS) \
    ../../lib/utils/libABRT_web_utils.la \
    ../../lib/utils/libABRTdUtils.la \
    ../../lib/utils/libABRTUtils.la

abrt_action_rhtsupport_SOURCES = \
    abrt_rh_support.h abrt_rh_support.c \
    abrt-action-rhtsupport.cpp
abrt_action_rhtsupport_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(XMLRPC_CFLAGS) $(XMLRPC_CLIENT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_rhtsupport_LDFLAGS = -ltar
abrt_action_rhtsupport_LDADD = \
    $(GLIB_LIBS) \
    $(XMLRPC_LIBS) $(XMLRPC_CLIENT_LIBS) \
    ../../lib/utils/libABRT_web_utils.la \
    ../../lib/utils/libABRTdUtils.la \
    ../../lib/utils/libABRTUtils.la

abrt_action_kerneloops_SOURCES = \
    abrt-action-kerneloops.cpp
abrt_action_kerneloops_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
# libABRTdUtils is used only because of LoadPluginSettings:
abrt_action_kerneloops_LDADD = \
    ../../lib/utils/libABRT_web_utils.la \
    ../../lib/utils/libABRTdUtils.la \
    ../../lib/utils/libABRTUtils.la

abrt_action_print_SOURCES = \
    abrt-action-print.cpp
abrt_action_print_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
# libABRTdUtils is used only because of make_description_logger:
abrt_action_print_LDADD = \
    ../../lib/utils/libABRTdUtils.la \
    ../../lib/utils/libABRTUtils.la

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
