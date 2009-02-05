lib_LTLIBRARIES = libMiddleWare.la
libMiddleWare_la_SOURCES = MiddleWare.cpp MiddleWare.h PluginManager.cpp \
                           PluginManager.h CrashCatcherPlugin.cpp \
                           CrashCatcherPlugin.h DynamicLibrary.cpp \
                           DynamicLibrary.h Settings.h Settings.cpp
libMiddleWare_la_LIBADD = $(DL_LIBS) ../Utils/libUtils.la
libMiddleWare_la_LDFLAGS = -version-info 0:1:0
libMiddleWare_la_CPPFLAGS = -I$(srcdir)/../Utils

check_PROGRAMS = test
test_SOURCES = test.cpp
test_LDADD = ../Utils/libUtils.la libMiddleWare.la $(DL_LIBS)
test_CPPFLAGS = -I$(srcdir)/../Utils \
                -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
                -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
                -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"