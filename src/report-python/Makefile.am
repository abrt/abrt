pyreportexecdir = $(pyexecdir)/report

pyreportexec_PYTHON = __init__.py

pyreportexec_LTLIBRARIES = _pyreport.la

_pyreport_la_SOURCES = \
    reportmodule.c \
    crash_dump.c \
    dump_dir.c \
    common.h
_pyreport_la_CPPFLAGS = \
    -I$(srcdir)/../include/report -I$(srcdir)/../include \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DLOCALSTATEDIR='"$(localstatedir)"' \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    $(GLIB_CFLAGS) \
    $(PYTHON_CFLAGS) \
    -D_GNU_SOURCE \
    -Wall -Werror
_pyreport_la_LDFLAGS = \
    -module \
    -avoid-version \
    -export-symbols-regex init_pyreport
_pyreport_la_LIBADD = \
    ../lib/libreport.la
