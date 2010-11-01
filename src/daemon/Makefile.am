bin_SCRIPTS = \
    abrt-action-install-debuginfo \
    abrt-handle-upload

sbin_PROGRAMS = abrtd \
    abrt-server \
    abrt-action-analyze-c \
    abrt-action-analyze-python \
    abrt-action-analyze-oops \
    abrt-action-generate-backtrace \
    abrt-action-save-package-data

bin_PROGRAMS = \
    abrt-action-bugzilla \
    abrt-action-rhtsupport \
    abrt-action-kerneloops \
    abrt-action-print

abrtd_SOURCES = \
    PluginManager.h PluginManager.cpp \
    MiddleWare.h MiddleWare.cpp \
    CrashWatcher.h CrashWatcher.cpp \
    CommLayerServer.h CommLayerServer.cpp \
    CommLayerServerDBus.h CommLayerServerDBus.cpp \
    Settings.h Settings.cpp \
    Daemon.h Daemon.cpp
abrtd_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(DBUS_CFLAGS) \
    $(ENABLE_SOCKET_OR_DBUS) \
    -D_GNU_SOURCE \
    -Wall
abrtd_LDADD = \
    $(DL_LIBS) \
    $(DBUS_LIBS) \
    ../../lib/utils/libABRTUtils.la \
    ../../lib/utils/libABRTdUtils.la

abrt_server_SOURCES = \
    abrt-server.c
abrt_server_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_server_LDADD = \
    ../../lib/utils/libABRTUtils.la

abrt_action_analyze_c_SOURCES = \
    abrt-action-analyze-c.c
abrt_action_analyze_c_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_analyze_c_LDADD = \
    ../../lib/utils/libABRTUtils.la

abrt_action_analyze_python_SOURCES = \
    abrt-action-analyze-python.c
abrt_action_analyze_python_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_analyze_python_LDADD = \
    ../../lib/utils/libABRTUtils.la

abrt_action_analyze_oops_SOURCES = \
    abrt-action-analyze-oops.c
abrt_action_analyze_oops_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_analyze_oops_LDADD = \
    ../../lib/utils/libABRTUtils.la

abrt_action_generate_backtrace_SOURCES = \
    abrt-action-generate-backtrace.c
abrt_action_generate_backtrace_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_generate_backtrace_LDADD = \
    ../../lib/utils/libABRTUtils.la \
    ../btparser/libbtparser.la

abrt_action_save_package_data_SOURCES = \
    rpm.h rpm.c \
    Settings.h Settings.cpp \
    abrt-action-save-package-data.cpp
abrt_action_save_package_data_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
# polkit_check_authorization is in libABRTdUtils
abrt_action_save_package_data_LDADD = \
    $(RPM_LIBS) \
    ../../lib/utils/libABRTdUtils.la \
    ../../lib/utils/libABRTUtils.la

abrt_action_bugzilla_SOURCES = \
    abrt-action-bugzilla.cpp
abrt_action_bugzilla_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_bugzilla_LDADD = \
    $(GLIB_LIBS) \
    ../../lib/utils/libABRT_web_utils.la \
    ../../lib/utils/libABRTdUtils.la \
    ../../lib/utils/libABRTUtils.la

abrt_action_rhtsupport_SOURCES = \
    abrt_rh_support.h abrt_rh_support.c \
    abrt-action-rhtsupport.cpp
abrt_action_rhtsupport_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(XMLRPC_CFLAGS) $(XMLRPC_CLIENT_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
abrt_action_rhtsupport_LDFLAGS = -ltar
abrt_action_rhtsupport_LDADD = \
    $(GLIB_LIBS) \
    $(XMLRPC_LIBS) $(XMLRPC_CLIENT_LIBS) \
    ../../lib/utils/libABRT_web_utils.la \
    ../../lib/utils/libABRTdUtils.la \
    ../../lib/utils/libABRTUtils.la

abrt_action_kerneloops_SOURCES = \
    abrt-action-kerneloops.cpp
abrt_action_kerneloops_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
# libABRTdUtils is used only because of LoadPluginSettings:
abrt_action_kerneloops_LDADD = \
    ../../lib/utils/libABRT_web_utils.la \
    ../../lib/utils/libABRTdUtils.la \
    ../../lib/utils/libABRTUtils.la

abrt_action_print_SOURCES = \
    abrt-action-print.cpp
abrt_action_print_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/utils \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DDEBUG_INFO_DIR=\"$(DEBUG_INFO_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
# libABRTdUtils is used only because of make_description_logger:
abrt_action_print_LDADD = \
    ../../lib/utils/libABRTdUtils.la \
    ../../lib/utils/libABRTUtils.la

dbusabrtconfdir = ${sysconfdir}/dbus-1/system.d/
dist_dbusabrtconf_DATA = dbus-abrt.conf

daemonconfdir = $(CONF_DIR)
dist_daemonconf_DATA = \
    abrt.conf \
    abrt_action.conf \
    gpg_keys

polkitconfdir = ${datadir}/polkit-1/actions
dist_polkitconf_DATA = org.fedoraproject.abrt.policy

comredhatabrtservicedir = ${datadir}/dbus-1/system-services
dist_comredhatabrtservice_DATA = com.redhat.abrt.service

man_MANS = abrtd.8 abrt.conf.5

EXTRA_DIST = $(man_MANS) abrt-action-install-debuginfo abrt-handle-upload

if HAVE_SYSTEMD
dist_systemdsystemunit_DATA = \
        abrtd.service
endif

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

install-data-local:
	$(mkdir_p) '$(DESTDIR)/$(VAR_RUN)'
