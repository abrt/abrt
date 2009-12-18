# ABRTUtils has small set of deps. This reduces deps of smaller abrt binaries
# ABRTdUtils has much more. It is used by daemon and plugins only
lib_LTLIBRARIES = libABRTUtils.la libABRTdUtils.la

# Not used just yet:
# time.cpp
# xconnect.cpp

# removed: CrashTypesSocket.cpp
libABRTUtils_la_SOURCES = \
    xfuncs.cpp \
    encbase64.cpp \
    read_write.cpp \
    logging.cpp \
    copyfd.cpp \
    daemon.cpp \
    skip_whitespace.cpp \
    xatonum.cpp \
    popen_and_save_output.cpp \
    stringops.cpp \
    dirsize.cpp \
    DebugDump.h DebugDump.cpp \
    ABRTException.cpp
libABRTUtils_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../../inc \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE
libABRTUtils_la_LDFLAGS = \
    -version-info 0:1:0 \
    $(DL_LIBS) \
libABRTUtils_la_LIBADD =


libABRTdUtils_la_SOURCES = \
    parse_release.cpp \
    make_descr.cpp \
    CommLayerInner.h CommLayerInner.cpp \
    abrt_dbus.h abrt_dbus.cpp \
    abrt_xmlrpc.h abrt_xmlrpc.cpp \
    Plugin.h Plugin.cpp \
    Polkit.h Polkit.cpp \
    Action.h Database.h Reporter.h Analyzer.h \
    Observer.h \
    DBusCommon.h
libABRTdUtils_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../../inc \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(XMLRPC_CFLAGS) $(XMLRPC_CLIENT_CFLAGS) \
    $(POLKIT_CFLAGS) \
    -D_GNU_SOURCE
libABRTdUtils_la_LDFLAGS = \
    -version-info 0:1:0 \
    $(DL_LIBS) \
    $(DBUS_LIBS)
libABRTdUtils_la_LIBADD = \
    $(XMLRPC_LIBS) $(XMLRPC_CLIENT_LIBS) \
    $(POLKIT_LIBS)

$(DESTDIR)/$(DEBUG_DUMPS_DIR):
	$(mkdir_p) '$@'
# no need to chmod it here
#chmod 1777 '$@'
install-data-local: $(DESTDIR)/$(DEBUG_DUMPS_DIR)
