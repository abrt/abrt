libexec_PROGRAMS = hookCCpp

# CCpp
hookCCpp_SOURCES = CCpp.cpp
hookCCpp_LDADD = ../../lib/Utils/libABRTUtils.la
hookCCpp_CPPFLAGS = -I$(srcdir)/../../inc -I$(srcdir)/../../lib/Utils \
                    -DDEBUG_DUMPS_DIR=\"$(DEBUG_DUMPS_DIR)\"
                    
python_PYTHON = sitecustomize.py abrt_exception_handler.py

EXTRA_DIST = pyhook.conf

# FIXME: remove hardcoded path
pyhookconfdir = /etc/abrt
dist_pyhookconf_DATA = pyhook.conf

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))

install-data-local:
	sed s,@DEBUG_DUMP_DIR@,$(DEBUG_DUMPS_DIR),g abrt_exception_handler.py.in  > abrt_exception_handler.py
