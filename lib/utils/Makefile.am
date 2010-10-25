# ABRTUtils has small set of deps. This reduces deps of smaller abrt binaries
# ABRTdUtils has much more. It is used by daemon and plugins only
# ABRT_web_utils is used only by some abrt-action-foo programs, not by daemon
lib_LTLIBRARIES = \
    libABRTUtils.la \
    libABRTdUtils.la \
    libABRT_web_utils.la
HEADER_DIR = $(srcdir)/../../inc
AM_CPPFLAGS = -I$(HEADER_DIR)

# Not used just yet:
# time.cpp
# xconnect.cpp

libABRTUtils_la_SOURCES = \
    xfuncs.c \
    concat_path_file.c \
    append_to_malloced_string.c \
    encbase64.c \
    stdio_helpers.c \
    hash_md5.c hash_md5.h \
    hash_sha1.c hash_sha1.h \
    read_write.c read_write.h \
    logging.c logging.h \
    copyfd.c \
    daemon.c \
    skip_whitespace.c \
    xatonum.c numtoa.cpp \
    spawn.c \
    stringops.cpp \
    dirsize.c \
    dump_dir.c \
    abrt_dbus.c abrt_dbus.h \
    CrashTypes.cpp \
    ABRTException.cpp \
    strbuf.c strbuf.h \
    abrt_packages.c abrt_packages.h \
    hooklib.c hooklib.h \
    database.c \
    parse_options.c parse_options.h
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
    -D_GNU_SOURCE
libABRTUtils_la_LDFLAGS = \
    -version-info 0:1:0
libABRTUtils_la_LIBADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS) \
    -ldl

libABRTdUtils_la_SOURCES = \
    parse_release.cpp \
    make_descr.cpp \
    $(HEADER_DIR)/comm_layer_inner.h CommLayerInner.cpp \
    $(HEADER_DIR)/plugin.h Plugin.cpp \
    Polkit.h Polkit.cpp
libABRTdUtils_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../../inc \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(POLKIT_CFLAGS) \
    -D_GNU_SOURCE
libABRTdUtils_la_LDFLAGS = \
    -version-info 0:1:0
libABRTdUtils_la_LIBADD = \
    $(POLKIT_LIBS) \
    -ldl

libABRT_web_utils_la_SOURCES = \
    abrt_curl.h abrt_curl.c \
    abrt_xmlrpc.h abrt_xmlrpc.cpp
libABRT_web_utils_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../../inc \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(CURL_CFLAGS) \
    $(LIBXML_CFLAGS) \
    $(XMLRPC_CFLAGS) $(XMLRPC_CLIENT_CFLAGS) \
    -D_GNU_SOURCE
libABRT_web_utils_la_LDFLAGS = \
    -version-info 0:1:0
libABRT_web_utils_la_LIBADD = \
    $(CURL_LIBS) \
    $(LIBXML_LIBS) \
    $(XMLRPC_LIBS) $(XMLRPC_CLIENT_LIBS)

$(DESTDIR)/$(DEBUG_DUMPS_DIR):
	$(mkdir_p) '$@'
# no need to chmod it here
#chmod 1777 '$@'
install-data-local: $(DESTDIR)/$(DEBUG_DUMPS_DIR)
