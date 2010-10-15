AM_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../utils
pluginslibdir=$(PLUGINS_LIB_DIR)
pluginslib_LTLIBRARIES = \
    libCCpp.la \
    libMailx.la \
    libSQLite3.la \
    libLogger.la \
    libKerneloopsScanner.la\
    libKerneloops.la \
    libKerneloopsReporter.la \
    libRunApp.la \
    libSOSreport.la \
    libBugzilla.la \
    libRHTSupport.la \
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
    abrt-RunApp.7 \
    abrt-ReportUploader.7

EXTRA_DIST = $(man_MANS)

$(DESTDIR)/$(DEBUG_INFO_DIR):
	$(mkdir_p) '$@'

install-data-hook: $(DESTDIR)/$(DEBUG_INFO_DIR)
	sed 's: = /var/: = $(localstatedir)/:g' -i \
	    $(DESTDIR)$(sysconfdir)/abrt/plugins/SQLite3.conf \
	    $(DESTDIR)$(sysconfdir)/abrt/plugins/Logger.conf

INC_PATH=$(srcdir)/../../inc
UTILS_PATH=$(srcdir)/../utils

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

# KerneloopsReporter
libKerneloopsReporter_la_SOURCES = KerneloopsReporter.cpp KerneloopsReporter.h
libKerneloopsReporter_la_LDFLAGS = -avoid-version
libKerneloopsReporter_la_LIBADD = $(CURL_LIBS)
libKerneloopsReporter_la_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH) $(CURL_CFLAGS) -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\"

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

# Logger
libLogger_la_SOURCES = Logger.cpp Logger.h
libLogger_la_LDFLAGS = -avoid-version
libLogger_la_CPPFLAGS = -I$(INC_PATH) -I$(UTILS_PATH) -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\"

# RunApp
libRunApp_la_SOURCES = RunApp.h RunApp.cpp
libRunApp_la_LDFLAGS = -avoid-version

# SOSreport
libSOSreport_la_SOURCES = SOSreport.cpp SOSreport.h
libSOSreport_la_LDFLAGS = -avoid-version

# Bugzilla
libBugzilla_la_SOURCES = Bugzilla.h Bugzilla.cpp
libBugzilla_la_LIBADD =
libBugzilla_la_LDFLAGS = -avoid-version
libBugzilla_la_CPPFLAGS = \
    -I$(INC_PATH) -I$(UTILS_PATH) \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\"

# RHTSupport
libRHTSupport_la_SOURCES = RHTSupport.h RHTSupport.cpp
libRHTSupport_la_LIBADD =
libRHTSupport_la_LDFLAGS = -avoid-version -ltar
libRHTSupport_la_CPPFLAGS = \
    -I$(INC_PATH) -I$(UTILS_PATH) \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"'

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
