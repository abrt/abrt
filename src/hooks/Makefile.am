pluginsconfdir = $(PLUGINS_CONF_DIR)

dist_pluginsconf_DATA = \
    CCpp.conf

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
    -DCONF_DIR=\"$(CONF_DIR)\" \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -Wall -Wwrite-strings \
    -D_GNU_SOURCE
abrt_hook_ccpp_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

python_PYTHON = abrt.pth abrt_exception_handler.py
EXTRA_DIST = abrt_exception_handler.py.in abrt-install-ccpp-hook $(man_MANS)

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))

# Must be synchronized with another sed call below.
abrt_exception_handler.py:
	sed s,\@VAR_RUN\@,\"$(VAR_RUN)\",g abrt_exception_handler.py.in > abrt_exception_handler.py

# RPM fix: we need to regenerate abrt_exception_handler.py, because it has the default ddir
install-data-local:
	sed s,\@VAR_RUN\@,\"$(VAR_RUN)\",g abrt_exception_handler.py.in > abrt_exception_handler.py

if HAVE_SYSTEMD
dist_systemdsystemunit_DATA = \
    abrt-ccpp.service \
    abrt-vmcore.service
endif
