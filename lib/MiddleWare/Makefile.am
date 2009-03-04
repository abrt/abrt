lib_LTLIBRARIES = libMiddleWare.la
libMiddleWare_la_SOURCES = MiddleWare.cpp MiddleWare.h PluginManager.cpp \
                           PluginManager.h ABRTPlugin.cpp \
                           ABRTPlugin.h DynamicLibrary.cpp \
                           DynamicLibrary.h Settings.h Settings.cpp \
                           RPM.cpp RPM.h Plugin.h CrashTypes.h \
                           MiddleWareTypes.h Application.h Database.h \
                           Reporter.h Language.h
libMiddleWare_la_LIBADD = $(DL_LIBS) ../Utils/libUtils.la  $(RPM_LIBS)
libMiddleWare_la_LDFLAGS = -version-info 0:1:0
libMiddleWare_la_CPPFLAGS = -I$(srcdir)/../Utils  $(RPM_CFLAGS)

middlewareconfdir = $(CONF_DIR)
dist_middlewareconf_DATA = abrt.conf

check_PROGRAMS = test
test_SOURCES = test.cpp
test_LDADD = ../Utils/libUtils.la libMiddleWare.la $(DL_LIBS) $(RPM_LIBS)
test_CPPFLAGS = -I$(srcdir)/../Utils \
                -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
                -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
                -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
                -DCONF_DIR=\"$(CONF_DIR)\"
