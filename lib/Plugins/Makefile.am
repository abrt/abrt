AM_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils -I$(srcdir)/../CommLayer
pluginslibdir=$(PLUGINS_LIB_DIR)
pluginslib_LTLIBRARIES = libCCpp.la \
                         libMailx.la \
                         libSQLite3.la \
                         libLogger.la \
                         libKerneloopsScanner.la\
                         libKerneloopsReporter.la\
                         libKerneloops.la \
                         libRunApp.la \
                         libBugzilla.la \
                         libPython.la
                          
pluginsconfdir=$(PLUGINS_CONF_DIR)
dist_pluginsconf_DATA = CCpp.conf Mailx.conf SQLite3.conf Logger.conf KerneloopsScanner.conf KerneloopsReporter.conf Bugzilla.conf

# CCpp
libCCpp_la_SOURCES = CCpp.cpp CCpp.h PluginSettings.h
libCCpp_la_LDFLAGS = -avoid-version
libCCpp_la_LIBADD = $(NSS_LIBS)
libCCpp_la_CPPFLAGS =  -I$(srcdir)/../CommLayer -I$(srcdir)/../../inc -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils -DCCPP_HOOK_PATH=\"${libexecdir}/hookCCpp\" -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" $(NSS_CFLAGS)

# Kerneloops
libKerneloops_la_SOURCES = Kerneloops.cpp Kerneloops.h 
libKerneloops_la_LDFLAGS = -avoid-version
libKerneloops_la_CPPFLAGS = -I$(srcdir)/../CommLayer -I$(srcdir)/../../inc -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils

# KerneloopsReporter
libKerneloopsReporter_la_SOURCES = KerneloopsReporter.cpp KerneloopsReporter.h PluginSettings.h
libKerneloopsReporter_la_LDFLAGS = -avoid-version
libKerneloopsReporter_la_LIBADD = $(CURL_LIBS)
libKerneloopsReporter_la_CPPFLAGS = -I$(srcdir)/../CommLayer -I$(srcdir)/../../inc -I$(srcdir)/../MiddleWare $(CURL_CFLAGS)

# KerneloopsScanner
libKerneloopsScanner_la_SOURCES = KerneloopsScanner.cpp KerneloopsScanner.h KerneloopsSysLog.cpp KerneloopsSysLog.h PluginSettings.h
libKerneloopsScanner_la_LDFLAGS = -avoid-version
libKerneloopsScanner_la_CPPFLAGS = -I$(srcdir)/../CommLayer -I$(srcdir)/../../inc -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"

# Mailx
libMailx_la_SOURCES = Mailx.cpp Mailx.h PluginSettings.h
libMailx_la_LDFLAGS = -avoid-version

# SQLite3
libSQLite3_la_SOURCES = SQLite3.cpp SQLite3.h PluginSettings.h
libSQLite3_la_LDFLAGS = -avoid-version
libSQLite3_la_LIBADD = $(SQLITE3_LIBS)
libSQLite3_la_CPPFLAGS = -I$(srcdir)/../CommLayer -I$(srcdir)/../../inc -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils $(SQLITE3_CFLAGS)

# Logger
libLogger_la_SOURCES = Logger.cpp Logger.h PluginSettings.h
libLogger_la_LDFLAGS = -avoid-version

# RunApp
libRunApp_la_SOURCES = RunApp.h RunApp.cpp
libRunApp_la_LDFLAGS = -avoid-version

# Bugzilla
libBugzilla_la_SOURCES = Bugzilla.h Bugzilla.cpp
libBugzilla_la_LIBADD = $(XMLRPC_CPP_LIBS) $(XMLRPC_CLIENT_CPP_LIBS) $(NSS_LIBS)
libBugzilla_la_LDFLAGS = -avoid-version
libBugzilla_la_CPPFLAGS = $(XMLRPC_CPP_CFLAGS) $(XMLRPC_CLIENT_CPP_CFLAGS) $(NSS_CFLAGS) -I$(srcdir)/../CommLayer -I$(srcdir)/../../inc -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils

# Python
libPython_la_SOURCES = Python.h Python.cpp
#libPython_la_LIBADD = $(NSS_LIBS)
libPython_la_LDFLAGS = -avoid-version
libPython_la_CPPFLAGS = $(NSS_CFLAGS) -I$(srcdir)/../CommLayer -I$(srcdir)/../../inc -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils

