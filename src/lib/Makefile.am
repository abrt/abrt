# libabrt - the stuff shared among most of abrt (like xmalloc, logging)
# libabrt_daemon - only daemon related things are here
# libabrt_dbus - daemon, cli and applet use this
# libabrt_web - for abrt-action-foo where foo deals with network/web/ftp/...
lib_LTLIBRARIES = \
    libabrt.la \
    libabrt_daemon.la \
    libabrt_dbus.la \
    libabrt_web.la

HEADER_DIR = $(srcdir)/../include
AM_CPPFLAGS = -I$(HEADER_DIR)

# Not used just yet:
# time.cpp
# xconnect.cpp

libabrt_la_SOURCES = \
    xfuncs.c \
    encbase64.c \
    stdio_helpers.c \
    hash_md5.c hash_md5.h \
    hash_sha1.c hash_sha1.h \
    read_write.c read_write.h \
    logging.c logging.h \
    copyfd.c \
    concat_path_file.c \
    append_to_malloced_string.c \
    overlapping_strcpy.c \
    skip_whitespace.c \
    strbuf.c strbuf.h \
    xatonum.c numtoa.cpp \
    spawn.c \
    dirsize.c \
    dump_dir.c \
    get_cmdline.c \
    daemon_is_ok.c \
    load_plugin_settings.cpp \
    make_descr.cpp \
    run_event.c \
    crash_dump.cpp \
    create_crash_dump_dir.cpp \
    abrt_types.c \
    hooklib.c hooklib.h \
    parse_release.cpp \
    parse_options.c parse_options.h
libabrt_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../include \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE
libabrt_la_LDFLAGS = \
    -version-info 0:1:0
libabrt_la_LIBADD = \
    $(GLIB_LIBS)

libabrt_dbus_la_SOURCES = \
    abrt_dbus.c abrt_dbus.h
libabrt_dbus_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../include \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    -D_GNU_SOURCE
libabrt_dbus_la_LDFLAGS = \
    -version-info 0:1:0
libabrt_dbus_la_LIBADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS)

libabrt_daemon_la_SOURCES = \
    $(HEADER_DIR)/comm_layer_inner.h CommLayerInner.cpp \
    $(HEADER_DIR)/plugin.h Plugin.cpp
libabrt_daemon_la_CPPFLAGS = \
    -Wall \
    -I$(srcdir)/../include \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE
libabrt_daemon_la_LDFLAGS = \
    -version-info 0:1:0
libabrt_daemon_la_LIBADD = \
    -ldl

libabrt_web_la_SOURCES = \
    abrt_curl.h abrt_curl.c \
    abrt_xmlrpc.h abrt_xmlrpc.cpp
libabrt_web_la_CPPFLAGS = \
    -Wall -Werror \
    -I$(srcdir)/../include \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    $(CURL_CFLAGS) \
    $(LIBXML_CFLAGS) \
    $(XMLRPC_CFLAGS) $(XMLRPC_CLIENT_CFLAGS) \
    -D_GNU_SOURCE
libabrt_web_la_LDFLAGS = \
    -version-info 0:1:0
libabrt_web_la_LIBADD = \
    $(GLIB_LIBS) \
    $(CURL_LIBS) \
    $(LIBXML_LIBS) \
    $(XMLRPC_LIBS) $(XMLRPC_CLIENT_LIBS)

$(DESTDIR)/$(DEBUG_DUMPS_DIR):
	$(mkdir_p) '$@'
# no need to chmod it here
#chmod 1777 '$@'
install-data-local: $(DESTDIR)/$(DEBUG_DUMPS_DIR)
