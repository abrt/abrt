# libreport - the stuff shared among most of abrt (like xmalloc, logging)
# libabrt_dbus - daemon, cli and applet use this
# libabrt_web - for abrt-action-foo where foo deals with network/web/ftp/...
lib_LTLIBRARIES = \
    libreport.la \
    libabrt_dbus.la \
    libabrt_web.la

# Not used just yet:
# time.cpp
# xconnect.cpp

libreport_la_SOURCES = \
    xfuncs.c \
    encbase64.c \
    binhex.c \
    stdio_helpers.c \
    hash_sha1.c hash_sha1.h \
    read_write.c read_write.h \
    logging.c logging.h \
    copyfd.c \
    copy_file_recursive.c \
    concat_path_file.c \
    append_to_malloced_string.c \
    overlapping_strcpy.c \
    skip_whitespace.c \
    glib_support.c \
    iso_date_string.c \
    strbuf.c strbuf.h \
    xatonum.c \
    spawn.c \
    dirsize.c \
    dump_dir.c \
    get_cmdline.c \
    daemon_is_ok.c \
    load_plugin_settings.c \
    make_descr.c \
    run_event.c \
    problem_data.c \
    create_dump_dir.c \
    abrt_types.c \
    hooklib.c hooklib.h \
    parse_release.c \
    parse_options.c parse_options.h \
    abrt_conf.c abrt_conf.h \
    steal_directory.c \
    event_xml_parser.c \
    event_config.c
libreport_la_CPPFLAGS = \
    -Wall -Wwrite-strings -Werror \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DEVENTS_DIR=\"$(EVENTS_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE
libreport_la_LDFLAGS = \
    -version-info 0:1:0
libreport_la_LIBADD = \
    $(GLIB_LIBS)

libabrt_dbus_la_SOURCES = \
    abrt_dbus.c abrt_dbus.h
libabrt_dbus_la_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DEVENTS_DIR=\"$(EVENTS_DIR)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    -Wall -Wwrite-strings -Werror \
    -D_GNU_SOURCE
libabrt_dbus_la_LDFLAGS = \
    -version-info 0:1:0
libabrt_dbus_la_LIBADD = \
    $(GLIB_LIBS) \
    $(DBUS_LIBS)

libabrt_web_la_SOURCES = \
    abrt_curl.h abrt_curl.c \
    abrt_xmlrpc.h abrt_xmlrpc.cpp
libabrt_web_la_CPPFLAGS = \
    -Wall -Wwrite-strings -Werror \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DEVENTS_DIR=\"$(EVENTS_DIR)\" \
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
