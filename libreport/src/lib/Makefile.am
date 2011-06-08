# libreport - the stuff shared among most of abrt (like xmalloc, logging)
lib_LTLIBRARIES = \
    libreport.la \
    libabrt_dbus.la

# Not used just yet:
# time.cpp
# xconnect.cpp

libreport_la_SOURCES = \
    xfuncs.c \
    is_in_string_list.c \
    encbase64.c \
    binhex.c \
    stdio_helpers.c \
    hash_sha1.c \
    read_write.c \
    logging.c \
    copyfd.c \
    copy_file_recursive.c \
    concat_path_file.c \
    append_to_malloced_string.c \
    overlapping_strcpy.c \
    skip_whitespace.c \
    glib_support.c \
    iso_date_string.c \
    strbuf.c \
    xatonum.c \
    spawn.c \
    dirsize.c \
    dump_dir.c \
    get_cmdline.c \
    load_plugin_settings.c \
    make_descr.c \
    run_event.c \
    problem_data.c \
    create_dump_dir.c \
    abrt_types.c \
    parse_release.c \
    parse_options.c \
    steal_directory.c \
    event_xml_parser.c \
    event_config.c \
    kernel-tainted.c \
    report.c
libreport_la_CPPFLAGS = \
    -Wall -Wwrite-strings -Werror \
    -I$(srcdir)/../include \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DEVENTS_DIR=\"$(EVENTS_DIR)\" \
    -DBIN_DIR=\"$(bindir)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE
libreport_la_LDFLAGS = \
    -version-info 0:1:0
libreport_la_LIBADD = \
    $(GLIB_LIBS)

libreportconfdir = $(CONF_DIR)
dist_libreportconf_DATA = \
    report_event.conf

libabrt_dbus_la_SOURCES = \
    abrt_dbus.c
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
    libreport.la \
    $(GLIB_LIBS) \
    $(DBUS_LIBS)

$(DESTDIR)/$(DEBUG_DUMPS_DIR):
	$(mkdir_p) '$@'
# no need to chmod it here
#chmod 1777 '$@'
install-data-local: $(DESTDIR)/$(DEBUG_DUMPS_DIR)
