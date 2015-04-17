confdir = $(CONF_DIR)
pyhookdir = $(pyexecdir)

dist_conf_DATA = \
    abrt-harvest-vmcore.conf

pluginsconfdir = $(PLUGINS_CONF_DIR)

dist_pluginsconf_DATA = \
    CCpp.conf \
    python.conf \
    oops.conf

defaultpluginsconfdir = $(DEFAULT_PLUGINS_CONF_DIR)
dist_defaultpluginsconf_DATA = $(dist_pluginsconf_DATA)

sbin_SCRIPTS = \
    abrt-install-ccpp-hook \
    abrt-harvest-vmcore

libexec_PROGRAMS = abrt-hook-ccpp

# abrt-hook-ccpp
abrt_hook_ccpp_SOURCES = \
    abrt-hook-ccpp.c
abrt_hook_ccpp_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    $(LIBSELINUX_CFLAGS) \
    -Wall -Wwrite-strings \
    -Werror \
    -D_GNU_SOURCE
abrt_hook_ccpp_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS) \
    $(LIBSELINUX_LIBS)

pyhook_PYTHON = abrt_exception_handler.py abrt.pth
EXTRA_DIST = abrt_exception_handler.py.in \
	abrt-install-ccpp-hook \
	abrt-harvest-vmcore

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc)) $(man1_MANS)

# Generate on build
all: abrt_exception_handler.py

# Must be synchronized with another sed call below.
abrt_exception_handler.py: abrt_exception_handler.py.in
	sed -e s,\@VAR_RUN\@,\"$(VAR_RUN)\",g \
	    -e s,\@CONF_DIR\@,\"$(CONF_DIR)\",g \
	    abrt_exception_handler.py.in >abrt_exception_handler.py

# RPM fix: we need to regenerate abrt_exception_handler.py, because it has the default ddir
install-data-local:
	sed -e s,\@VAR_RUN\@,\"$(VAR_RUN)\",g \
	    -e s,\@CONF_DIR\@,\"$(CONF_DIR)\",g \
	    abrt_exception_handler.py.in >abrt_exception_handler.py
