lib_LTLIBRARIES = libABRTUtils.la
libABRTUtils_la_SOURCES = \
	DebugDump.cpp DebugDump.h \
	CrashTypesSocket.cpp \
	xfuncs.cpp \
	read_write.cpp \
	logging.cpp \
	copyfd.cpp \
	Polkit.cpp \
	Polkit.h

libABRTUtils_la_LDFLAGS = -version-info 0:1:0
libABRTUtils_la_LIBADD = -lmagic $(POLKIT_LIBS)
libABRTUtils_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../../lib/CommLayer $(POLKIT_CFLAGS)

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(DEBUG_DUMPS_DIR)'
	chmod 1777 '$(DESTDIR)/$(DEBUG_DUMPS_DIR)'
