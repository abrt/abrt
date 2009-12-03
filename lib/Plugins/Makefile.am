AM_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils
pluginslibdir=$(PLUGINS_LIB_DIR)
pluginslib_LTLIBRARIES = \
    libCCpp.la \
    libMailx.la \
    libSQLite3.la \
    libLogger.la \
    libKerneloopsScanner.la\
    libKerneloopsReporter.la\
    libKerneloops.la \
    libRunApp.la \
    libSOSreport.la \
    libBugzilla.la \
    libCatcut.la \
    libTicketUploader.la \
    libPython.la \
    libFileTransfer.la
#    libFirefox.la

dist_pluginslib_DATA = \
    KerneloopsReporter.GTKBuilder Logger.GTKBuilder \
    Mailx.GTKBuilder Bugzilla.GTKBuilder \
    TicketUploader.GTKBuilder Catcut.GTKBuilder

pluginsconfdir = $(PLUGINS_CONF_DIR)
dist_pluginsconf_DATA = \
    CCpp.conf \
    Mailx.conf \
    SQLite3.conf \
    Logger.conf \
    Kerneloops.conf \
    Bugzilla.conf \
    Catcut.conf \
    TicketUploader.conf \
    FileTransfer.conf

man_MANS = abrt-FileTransfer.7 abrt-Bugzilla.7 abrt-KerneloopsReporter.7 \
           abrt-KerneloopsScanner.7 abrt-Logger.7 abrt-Mailx.7  abrt-plugins.7 \
           abrt-SQLite3.7  abrt-RunApp.7 abrt-TicketUploader.7
# + abrt-Catcut.7

EXTRA_DIST = $(man_MANS)

$(DESTDIR)/$(DEBUG_INFO_DIR):
	$(mkdir_p) '$@'

install-data-hook: $(DESTDIR)/$(DEBUG_INFO_DIR)
	chmod 0600 $(DESTDIR)$(sysconfdir)/abrt/plugins/Bugzilla.conf
	sed 's: = /var/: = $(localstatedir)/:g' -i \
	    $(DESTDIR)$(sysconfdir)/abrt/plugins/SQLite3.conf \
	    $(DESTDIR)$(sysconfdir)/abrt/plugins/Logger.conf

# CCpp
libCCpp_la_SOURCES = CCpp.cpp CCpp.h
libCCpp_la_LDFLAGS = -avoid-version
libCCpp_la_LIBADD = $(NSS_LIBS)
libCCpp_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils \
    -DCCPP_HOOK_PATH=\"${libexecdir}/abrt-hook-ccpp\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    $(NSS_CFLAGS)


# Firefox - disabled for now
#libFirefox_la_SOURCES = Firefox.cpp Firefox.h
#libFirefox_la_LDFLAGS = -avoid-version
#libFirefox_la_LIBADD = $(NSS_LIBS)
#libFirefox_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils \
#    -DCCPP_HOOK_PATH=\"${libexecdir}/abrt-hook-ccpp\" \
#    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
#    -DLOCALSTATEDIR='"$(localstatedir)"' \
#    $(NSS_CFLAGS)

# Kerneloops
libKerneloops_la_SOURCES = Kerneloops.cpp Kerneloops.h
libKerneloops_la_LDFLAGS = -avoid-version
libKerneloops_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils

# KerneloopsReporter
libKerneloopsReporter_la_SOURCES = KerneloopsReporter.cpp KerneloopsReporter.h
libKerneloopsReporter_la_LDFLAGS = -avoid-version
libKerneloopsReporter_la_LIBADD = $(CURL_LIBS)
libKerneloopsReporter_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils $(CURL_CFLAGS) -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\"

# KerneloopsScanner
libKerneloopsScanner_la_SOURCES = KerneloopsScanner.cpp KerneloopsScanner.h KerneloopsSysLog.cpp KerneloopsSysLog.h
libKerneloopsScanner_la_LDFLAGS = -avoid-version
libKerneloopsScanner_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"

# Mailx
libMailx_la_SOURCES = Mailx.cpp Mailx.h
libMailx_la_LDFLAGS = -avoid-version
libMailx_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" -DLOCALSTATEDIR='"$(localstatedir)"'

# SQLite3
libSQLite3_la_SOURCES = SQLite3.cpp SQLite3.h
libSQLite3_la_LDFLAGS = -avoid-version
libSQLite3_la_LIBADD = $(SQLITE3_LIBS)
libSQLite3_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils $(SQLITE3_CFLAGS) -DLOCALSTATEDIR='"$(localstatedir)"'

# Logger
libLogger_la_SOURCES = Logger.cpp Logger.h
libLogger_la_LDFLAGS = -avoid-version
libLogger_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\"

# RunApp
libRunApp_la_SOURCES = RunApp.h RunApp.cpp
libRunApp_la_LDFLAGS = -avoid-version

# SOSreport
libSOSreport_la_SOURCES = SOSreport.cpp SOSreport.h
libSOSreport_la_LDFLAGS = -avoid-version

# Bugzilla
libBugzilla_la_SOURCES = Bugzilla.h Bugzilla.cpp
libBugzilla_la_LIBADD = $(XMLRPC_LIBS) $(XMLRPC_CLIENT_LIBS)
libBugzilla_la_LDFLAGS = -avoid-version
libBugzilla_la_CPPFLAGS = $(XMLRPC_CFLAGS) $(XMLRPC_CLIENT_CFLAGS) -I$(srcdir)/../../inc -I$(srcdir)/../Utils -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\"

# Catcut
libCatcut_la_SOURCES = Catcut.h Catcut.cpp
libCatcut_la_LIBADD = $(XMLRPC_LIBS) $(XMLRPC_CLIENT_LIBS)
libCatcut_la_LDFLAGS = -avoid-version
libCatcut_la_CPPFLAGS = $(XMLRPC_CFLAGS) $(XMLRPC_CLIENT_CFLAGS) -I$(srcdir)/../../inc -I$(srcdir)/../Utils -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\"

# TicketUploader
libTicketUploader_la_SOURCES = TicketUploader.h TicketUploader.cpp
libTicketUploader_la_LDFLAGS = -avoid-version
libTicketUploader_la_LIBADD = $(CURL_LIBS)
libTicketUploader_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils $(CURL_CFLAGS) -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\"

# Python
libPython_la_SOURCES = Python.h Python.cpp
#libPython_la_LIBADD = $(NSS_LIBS)
libPython_la_LDFLAGS = -avoid-version
libPython_la_CPPFLAGS = $(NSS_CFLAGS) -I$(srcdir)/../../inc -I$(srcdir)/../Utils

# FileTrasfer
libFileTransfer_la_SOURCES = FileTransfer.cpp FileTransfer.h
libFileTransfer_la_LDFLAGS = -avoid-version -ltar -lbz2 -lz
libFileTransfer_la_LIBADD = $(CURL_LIBS) $(ZIP_LIBS)
libFileTransfer_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils $(CURL_CFLAGS) -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"
