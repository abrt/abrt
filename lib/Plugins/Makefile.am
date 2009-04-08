AM_CPPFLAGS = -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils
pluginslibdir=$(PLUGINS_LIB_DIR)
pluginslib_LTLIBRARIES = libCCpp.la \
                         libMailx.la \
                         libSQLite3.la \
                         libLogger.la \
                         libKerneloopsReporter.la\
                         libKerneloops.la \
                         libRunApp.la
                          
pluginsconfdir=$(PLUGINS_CONF_DIR)
dist_pluginsconf_DATA = CCpp.conf Mailx.conf SQLite3.conf Logger.conf Kerneloops.conf KerneloopsReporter.conf

# CCpp
libCCpp_la_SOURCES = CCpp.cpp CCpp.h PluginSettings.h
libCCpp_la_LDFLAGS = -avoid-version
libCCpp_la_LIBADD = $(NSS_LIBS)
libCCpp_la_CPPFLAGS = -I$(srcdir)/../MiddleWare  -I$(srcdir)/../Utils -DCCPP_HOOK_PATH=\"${libexecdir}/hookCCpp\" -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" $(NSS_CFLAGS)

# Kerneloops
libKerneloops_la_SOURCES = Kerneloops.cpp Kerneloops.h KerneloopsSysLog.cpp KerneloopsSysLog.h PluginSettings.h
libKerneloops_la_LDFLAGS = -avoid-version
libKerneloops_la_CPPFLAGS = -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" -DCCPP_HOOK_PATH=\"${libexecdir}/hookKerneloopsoops\"

# KerneloopsReporter
libKerneloopsReporter_la_SOURCES = KerneloopsReporter.cpp KerneloopsReporter.h PluginSettings.h
libKerneloopsReporter_la_LDFLAGS = -avoid-version
libKerneloopsReporter_la_LIBADD = $(CURL_LIBS)
libKerneloopsReporter_la_CPPFLAGS = -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils $(CURL_CFLAGS)

# Mailx
libMailx_la_SOURCES = Mailx.cpp Mailx.h PluginSettings.h
libMailx_la_LDFLAGS = -avoid-version

# SQLite3
libSQLite3_la_SOURCES = SQLite3.cpp SQLite3.h PluginSettings.h
libSQLite3_la_LDFLAGS = -avoid-version
libSQLite3_la_LIBADD = $(SQLITE3_LIBS)
libSQLite3_la_CPPFLAGS = -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils $(SQLITE3_CFLAGS)

# Logger
libLogger_la_SOURCES = Logger.cpp Logger.h PluginSettings.h
libLogger_la_LDFLAGS = -avoid-version

# RunApp
libRunApp_la_SOURCES = RunApp.h RunApp.cpp
libRunApp_la_LDFLAGS = -avoid-version