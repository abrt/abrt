# libreport - the stuff shared among most of abrt (like libreport_xmalloc, logging)
# libabrt_dbus - daemon, cli and applet use this
# libabrt_web - for abrt-action-foo where foo deals with network/web/ftp/...
lib_LTLIBRARIES = \
    libabrt.la

libabrt_la_SOURCES = \
    libabrt_init.c \
    abrt_conf.c \
    hooklib.c \
    daemon_is_ok.c \
    notify_new_path.c \
    kernel.c \
    abrt_glib.c \
    abrt_glib.h \
    migrate_dirs.c \
    check_recent_crash_file.c \
    problem_api.c \
    problem_api_dbus.c \
    libabrt.sym

libabrt_la_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DEVENTS_DIR=\"$(EVENTS_DIR)\" \
    -DDEFAULT_DUMP_LOCATION=\"$(DEFAULT_DUMP_LOCATION)\" \
    -DGDB=\"$(GDB)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(GIO_CFLAGS) \
    $(SATYR_CFLAGS) \
    -D_GNU_SOURCE
libabrt_la_LDFLAGS = \
    -version-info 1:0:1

if HAVE_LD_VERSION_SCRIPT
libabrt_la_LDFLAGS += -Wl,--version-script=$(srcdir)/libabrt.sym
endif

libabrt_la_LIBADD = \
    $(GLIB_LIBS) \
    $(GIO_LIBS) \
    $(LIBREPORT_LIBS) \
    $(SATYR_LIBS)

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@
