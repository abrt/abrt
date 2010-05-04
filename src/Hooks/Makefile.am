libexec_PROGRAMS = abrt-hook-ccpp
bin_PROGRAMS = dumpoops

# abrt-hook-ccpp
abrt_hook_ccpp_SOURCES = abrt-hook-ccpp.cpp
abrt_hook_ccpp_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/Utils \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -D_GNU_SOURCE
abrt_hook_ccpp_LDADD = \
    ../../lib/Utils/libABRTUtils.la

# dumpoops
dumpoops_SOURCES = dumpoops.cpp
dumpoops_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/Utils \
    -I$(srcdir)/../../lib/Plugins \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -D_GNU_SOURCE
# build will succeed, but at runtime plugins do need ABRT*d*Utils
dumpoops_LDADD = \
    ../../lib/Utils/libABRTUtils.la \
    ../../lib/Utils/libABRTdUtils.la

python_PYTHON = abrt.pth abrt_exception_handler.py
EXTRA_DIST = abrt_exception_handler.py.in $(man_MANS)

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))

# Must be synchronized with another sed call below.
abrt_exception_handler.py:
	sed s,\@VAR_RUN\@,\"$(VAR_RUN)\",g abrt_exception_handler.py.in > abrt_exception_handler.py

# RPM fix: we need to regenerate abrt_exception_handler.py, because it has the default ddir
install-data-local:
	sed s,\@VAR_RUN\@,\"$(VAR_RUN)\",g abrt_exception_handler.py.in > abrt_exception_handler.py
