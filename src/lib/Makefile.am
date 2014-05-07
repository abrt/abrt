# libreport - the stuff shared among most of abrt (like xmalloc, logging)
# libabrt_dbus - daemon, cli and applet use this
# libabrt_web - for abrt-action-foo where foo deals with network/web/ftp/...
lib_LTLIBRARIES = \
    libabrt.la

libabrt_la_SOURCES = \
    abrt_conf.c \
    hooklib.c \
    daemon_is_ok.c \
    kernel.c \
    check_recent_crash_file.c \
    problem_api.c

libabrt_la_CPPFLAGS = \
    -Wall -Wwrite-strings -Werror \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DDEFAULT_CONF_DIR=\"$(DEFAULT_CONF_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DDEFAULT_PLUGINS_CONF_DIR=\"$(DEFAULT_PLUGINS_CONF_DIR)\" \
    -DEVENTS_DIR=\"$(EVENTS_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(SATYR_CFLAGS) \
    -D_GNU_SOURCE
libabrt_la_LDFLAGS = \
    -version-info 0:1:0
libabrt_la_LIBADD = \
    $(GLIB_LIBS) \
    $(LIBREPORT_LIBS) \
    $(SATYR_LIBS)
