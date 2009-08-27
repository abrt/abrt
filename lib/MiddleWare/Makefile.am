lib_LTLIBRARIES = libABRTMiddleWare.la

libABRTMiddleWare_la_SOURCES = \
    Plugin.cpp Plugin.h \
    Action.h Database.h Reporter.h Analyzer.h
libABRTMiddleWare_la_LIBADD = \
    $(DL_LIBS) \
    ../Utils/libABRTUtils.la \
    $(RPM_LIBS)
libABRTMiddleWare_la_LDFLAGS = -version-info 0:1:0
libABRTMiddleWare_la_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../Utils \
    -I$(srcdir)/../CommLayer \
    $(RPM_CFLAGS)

# disabled: MiddleWare.* is moved to src/Daemon/ now
#
#check_PROGRAMS = test
#
#test_SOURCES = test.cpp
#test_LDADD = \
#    ../Utils/libABRTUtils.la \
#    ../CommLayer/libABRTCommLayer.la \
#    libABRTMiddleWare.la \
#    $(DL_LIBS) $(RPM_LIBS)
#test_CPPFLAGS = \
#    -I$(srcdir)/../../inc \
#    -I$(srcdir)/../Utils \
#    -I$(srcdir)/../CommLayer\
#    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
#    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
#    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
#    -DCONF_DIR=\"$(CONF_DIR)\"
