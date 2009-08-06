lib_LTLIBRARIES = libABRTCommLayer.la
libABRTCommLayer_la_SOURCES = CommLayerServer.h CommLayerServer.cpp \
                           CommLayerServerSocket.h CommLayerServerSocket.cpp \
                           CommLayerServerDBus.h CommLayerServerDBus.cpp \
                           DBusServerProxy.h Observer.h DBusCommon.h \
                           CommLayerInner.h CommLayerInner.cpp \
                           DBusClientProxy.h CommLayerClientDBus.h CommLayerClientDBus.cpp

libABRTCommLayer_la_LIBADD = ../../lib/MiddleWare/libABRTMiddleWare.la $(DL_LIBS) $(DBUSCPP_LIBS)
libABRTCommLayer_la_LDFLAGS = -version-info 0:1:0
libABRTCommLayer_la_CPPFLAGS = -Wall -Werror -I$(srcdir)/../../lib/MiddleWare\
                        -I$(srcdir)/../../lib/DBus -I$(srcdir)/../../inc\
                        -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" $(GLIB_CFLAGS) $(DBUSCPP_CFLAGS) \
                        -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
                        -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
                        -DCONF_DIR=\"$(CONF_DIR)\" -D_GNU_SOURCE \
                        -DVAR_RUN=\"$(VAR_RUN)\"

#check_PROGRAMS = test
#test_SOURCES = test.cpp
#test_LDADD = ../Utils/libABRTUtils.la libABRTCommLayer.la $(DL_LIBS) $(RPM_LIBS)
#test_CPPFLAGS = -I$(srcdir)/../Utils \
#                -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
#                -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
#                -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
#                -DCONF_DIR=\"$(CONF_DIR)\"
