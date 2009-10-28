lib_LTLIBRARIES = libABRTUtils.la

# Not used just yet:
# time.cpp
# xconnect.cpp
# skip_whitespace.cpp

libABRTUtils_la_SOURCES = \
    xfuncs.cpp \
    encbase64.cpp \
    read_write.cpp \
    logging.cpp \
    copyfd.cpp \
    CrashTypesSocket.cpp \
    DebugDump.h DebugDump.cpp \
    CommLayerInner.h CommLayerInner.cpp \
    abrt_dbus.h abrt_dbus.cpp \
    Plugin.h Plugin.cpp \
    Polkit.h Polkit.cpp \
    Action.h Database.h Reporter.h Analyzer.h \
    Observer.h \
    DBusCommon.h
libABRTUtils_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../../inc \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(POLKIT_CFLAGS) \
    -D_GNU_SOURCE
libABRTUtils_la_LDFLAGS = \
    -version-info 0:1:0 \
    $(DL_LIBS) \
    $(DBUS_LIBS)
libABRTUtils_la_LIBADD = \
    -lmagic \
    $(POLKIT_LIBS)

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(DEBUG_DUMPS_DIR)'
	chmod 1777 '$(DESTDIR)/$(DEBUG_DUMPS_DIR)'
