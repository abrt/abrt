lib_LTLIBRARIES = libUtils.la
libUtils_la_SOURCES = DebugDump.cpp DebugDump.h Packages.cpp Packages.h
libUtils_la_LDFLAGS = -version-info 0:1:0
libUtils_la_CPPFLAGS = $(PACKAGEKIT_GLIB_CFLAGS) $(RPM_CFLAGS)
libUtils_la_LIBADD = $(PACKAGEKIT_GLIB_LIBS) $(RPM_LIBS)

install-data-local:
	$(mkdir_p) '$(DEBUG_DUMPS_DIR)'