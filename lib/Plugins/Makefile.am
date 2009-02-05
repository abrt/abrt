AM_CPPFLAGS = -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils
pluginslibdir=$(PLUGINS_LIB_DIR)
pluginslib_LTLIBRARIES = libCCpp.la libMailx.la libSQLite3.la

pluginsconfdir=$(PLUGINS_CONF_DIR)
pluginsconf_DATA = CCpp.conf Mailx.conf SQLite3.conf

# CCpp
libCCpp_la_SOURCES = CCpp.cpp CCpp.h
libCCpp_la_LDFLAGS = -version-info 0:1:0
libCCpp_la_CPPFLAGS = -I$(srcdir)/../MiddleWare  -I$(srcdir)/../Utils -DCCPP_HOOK_PATH=\"${libexecdir}/hookCCpp\"

# Mailx
libMailx_la_SOURCES = Mailx.cpp Mailx.h
libMailx_la_LDFLAGS = -version-info 0:1:0

# SQLite3
libSQLite3_la_SOURCES = SQLite3.cpp SQLite3.h
libSQLite3_la_LDFLAGS = -version-info 0:1:0
libSQLite3_la_LIBADD = $(SQLITE3_LIBS)
libSQLite3_la_CPPFLAGS = -I$(srcdir)/../MiddleWare -I$(srcdir)/../Utils $(SQLITE3_CFLAGS)

