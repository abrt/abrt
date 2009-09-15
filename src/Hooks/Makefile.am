libexec_PROGRAMS = hookCCpp
bin_PROGRAMS = dumpoops

# CCpp
hookCCpp_SOURCES = CCpp.cpp
hookCCpp_LDADD = ../../lib/Utils/libABRTUtils.la
hookCCpp_CPPFLAGS = -I$(srcdir)/../../inc \
                    -I$(srcdir)/../../lib/Utils \
                    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
                    -DVAR_RUN=\"$(VAR_RUN)\"

# dumpoops
dumpoops_SOURCES = \
    dumpoops.cpp
dumpoops_LDADD = \
    ../../lib/Utils/libABRTUtils.la
dumpoops_CPPFLAGS = \
    -I$(srcdir)/../../inc \
    -I$(srcdir)/../../lib/Utils \
    -I$(srcdir)/../../lib/Plugins \
    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\" \
    -DPLUGINS_LIB_DIR=\"$(PLUGINS_LIB_DIR)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\"

python_PYTHON = sitecustomize.py abrt_exception_handler.py
EXTRA_DIST = abrt_exception_handler.py.in

pyhookconfdir = $(CONF_DIR)
dist_pyhookconf_DATA = pyhook.conf

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))

abrt_exception_handler.py:
	sed s,@DEBUG_DUMP_DIR@,$(DEBUG_DUMPS_DIR),g abrt_exception_handler.py.in  > abrt_exception_handler.py

# RPM fix: we need to regenerate abrt_exception_handler.py, because it has the default ddir
install-data-local:
	sed s,@DEBUG_DUMP_DIR@,$(DEBUG_DUMPS_DIR),g abrt_exception_handler.py.in  > abrt_exception_handler.py
