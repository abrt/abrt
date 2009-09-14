lib_LTLIBRARIES = libABRTCommLayer.la

libABRTCommLayer_la_SOURCES = \
    DBusClientProxy.h DBusClientProxy.cpp \
    CommLayerInner.h CommLayerInner.cpp \
    Observer.h \
    DBusCommon.h
libABRTCommLayer_la_LIBADD = \
    ../../lib/MiddleWare/libABRTMiddleWare.la \
    $(DL_LIBS) $(DBUSCPP_LIBS)
libABRTCommLayer_la_LDFLAGS = -version-info 0:1:0
libABRTCommLayer_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/MiddleWare \
    -I$(srcdir)/../../lib/DBus \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) $(DBUSCPP_CFLAGS) \
    -D_GNU_SOURCE

#check_PROGRAMS = test
#test_SOURCES = test.cpp
#test_LDADD = ../Utils/libABRTUtils.la libABRTCommLayer.la $(DL_LIBS) $(RPM_LIBS)
#test_CPPFLAGS = -I$(srcdir)/../Utils \
#                -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
#                -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
#                -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
#                -DCONF_DIR=\"$(CONF_DIR)\"
