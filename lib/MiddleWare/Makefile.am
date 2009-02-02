lib_LTLIBRARIES = libMiddleWare.la
libMiddleWare_la_SOURCES = MiddleWare.cpp MiddleWare.h PluginManager.cpp \
                           PluginManager.h CrashCatcherPlugin.cpp \
                           CrashCatcherPlugin.h DynamicLibrary.cpp \
                           DynamicLibrary.h
libMiddleWare_la_LIBADD = $(DL_LIBS)
libMiddleWare_la_LDFLAGS = -version-info 0:1:0


check_PROGRAMS = test
test_SOURCES = test.cpp
test_LDADD = ../DebugDump/libDebugDump.la libMiddleWare.la $(DL_LIBS)
test_CPPFLAGS = -I$(srcdir)/../DebugDump \
                -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
                -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
                -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"