lib_LTLIBRARIES = libABRTMiddleWare.la
libABRTMiddleWare_la_SOURCES = MiddleWare.cpp MiddleWare.h PluginManager.cpp \
                           PluginManager.h ABRTPlugin.cpp \
                           ABRTPlugin.h DynamicLibrary.cpp \
                           DynamicLibrary.h \
                           RPM.cpp RPM.h Plugin.h \
                           MiddleWareTypes.h Action.h Database.h \
                           Reporter.h Analyzer.h
libABRTMiddleWare_la_LIBADD = $(DL_LIBS) ../Utils/libABRTUtils.la  $(RPM_LIBS)
libABRTMiddleWare_la_LDFLAGS = -version-info 0:1:0
libABRTMiddleWare_la_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../Utils -I$(srcdir)/../CommLayer $(RPM_CFLAGS)

check_PROGRAMS = test
test_SOURCES = test.cpp
test_LDADD = ../Utils/libABRTUtils.la libABRTMiddleWare.la $(DL_LIBS) $(RPM_LIBS)
test_CPPFLAGS = -I$(srcdir)/../Utils \
                -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
                -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
                -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
                -DCONF_DIR=\"$(CONF_DIR)\"
