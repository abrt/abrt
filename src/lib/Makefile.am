# libreport - the stuff shared among most of abrt (like xmalloc, logging)
# libabrt_dbus - daemon, cli and applet use this
# libabrt_web - for abrt-action-foo where foo deals with network/web/ftp/...
lib_LTLIBRARIES = \
    libabrt.la

# Not used just yet:
# time.cpp
# xconnect.cpp

libabrt_la_SOURCES = \
    abrt_conf.c abrt_conf.h \
    hooklib.c hooklib.h \
    daemon_is_ok.c

libabrt_la_CPPFLAGS = \
    -Wall -Wwrite-strings -Werror \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DEVENTS_DIR=\"$(EVENTS_DIR)\" \
    -DBIN_DIR=\"$(bindir)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
libabrt_la_LDFLAGS = \
    -version-info 0:1:0
libabrt_la_LIBADD = \
    $(GLIB_LIBS)

$(DESTDIR)/$(DEBUG_DUMPS_DIR):
	$(mkdir_p) '$@'
# no need to chmod it here
#chmod 1777 '$@'
install-data-local: $(DESTDIR)/$(DEBUG_DUMPS_DIR)
