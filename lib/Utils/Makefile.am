lib_LTLIBRARIES = libABRTUtils.la
libABRTUtils_la_SOURCES = DebugDump.cpp DebugDump.h logging.cpp read_write.cpp xfuncs.cpp
libABRTUtils_la_LDFLAGS = -version-info 0:1:0
libABRTUtils_la_LIBADD = -lmagic
libABRTUtils_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../../lib/CommLayer

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(DEBUG_DUMPS_DIR)'
	chmod 1777 '$(DESTDIR)/$(DEBUG_DUMPS_DIR)'
