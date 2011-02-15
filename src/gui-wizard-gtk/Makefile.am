bin_PROGRAMS = bug-reporting-wizard

bug_reporting_wizard_SOURCES = \
    wizard.h wizard.c \
    main.c

#-Wl,--export-dynamic and lgmodule-2.0
#is required for gtk_builder_connect_signals() to work correctly
bug_reporting_wizard_CFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DBIN_DIR=\"$(bindir)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DICON_DIR=\"${datadir}/abrt/icons/hicolor/48x48/status\" \
    $(GTK_CFLAGS) \
    -D_GNU_SOURCE \
    -g \
    -Wl,--export-dynamic \
    -Wall -Werror

bug_reporting_wizard_LDADD = \
    ../lib/libreport.la \
    ../lib/libabrt_dbus.la \
    -lglib-2.0 \
    -lgthread-2.0 \
    -lgmodule-2.0 \
    $(GTK_LIBS)

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

@INTLTOOL_DESKTOP_RULE@
