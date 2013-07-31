# libreport - the stuff shared among most of abrt (like xmalloc, logging)
# libabrt_dbus - daemon, cli and applet use this
# libabrt_web - for abrt-action-foo where foo deals with network/web/ftp/...
lib_LTLIBRARIES = \
    libabrt.la

libabrt_la_SOURCES = \
    abrt_conf.c \
    hooklib.c \
    daemon_is_ok.c \
    notify_new_path.c \
    kernel.c \
    abrt_glib.c \
    migrate_dirs.c \
    check_recent_crash_file.c \
    problem_api.c \
    problem_api_dbus.c \
    ignored_problems.c

libabrt_la_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DEVENTS_DIR=\"$(EVENTS_DIR)\" \
    -DDEFAULT_DUMP_LOCATION=\"$(DEFAULT_DUMP_LOCATION)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(GIO_CFLAGS) \
    -D_GNU_SOURCE
libabrt_la_LDFLAGS = \
    -version-info 0:1:0
libabrt_la_LIBADD = \
    $(GLIB_LIBS) \
    $(GIO_LIBS) \
    $(LIBREPORT_LIBS)
