lib_LTLIBRARIES = libABRTUtils.la
libABRTUtils_la_SOURCES = DebugDump.cpp DebugDump.h
libABRTUtils_la_LDFLAGS = -version-info 0:1:0
libABRTUtils_la_LIBADD = -lmagic

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(DEBUG_DUMPS_DIR)'