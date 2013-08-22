confdir = $(CONF_DIR)
pyhookdir = $(pyexecdir)

dist_conf_DATA = \
    abrt-harvest-vmcore.conf

pluginsconfdir = $(PLUGINS_CONF_DIR)

dist_pluginsconf_DATA = \
    CCpp.conf \
    python.conf

sbin_SCRIPTS = \
    abrt-install-ccpp-hook \
    abrt-harvest-vmcore \
    abrt-harvest-pstoreoops

bin_PROGRAMS = \
    abrt-merge-pstoreoops

libexec_PROGRAMS = abrt-hook-ccpp

# abrt-hook-ccpp
abrt_hook_ccpp_SOURCES = \
    abrt-hook-ccpp.c
abrt_hook_ccpp_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
abrt_hook_ccpp_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

# abrt-merge-pstoreoops
abrt_merge_pstoreoops_SOURCES = \
    abrt-merge-pstoreoops.c
abrt_merge_pstoreoops_CPPFLAGS = \
    -I$(srcdir)/../include \
    -I$(srcdir)/../lib \
    -DVAR_RUN=\"$(VAR_RUN)\" \
    -DPLUGINS_CONF_DIR=\"$(PLUGINS_CONF_DIR)\" \
    -DDEFAULT_DUMP_DIR_MODE=$(DEFAULT_DUMP_DIR_MODE) \
    $(GLIB_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -D_GNU_SOURCE
abrt_merge_pstoreoops_LDADD = \
    ../lib/libabrt.la \
    $(LIBREPORT_LIBS)

DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

pyhook_PYTHON = \
	abrt_exception_handler.py \
	abrt.pth

EXTRA_DIST = abrt_exception_handler.py.in \
	abrt-install-ccpp-hook.in \
	abrt_harvest_vmcore.py.in \
	abrt-harvest-pstoreoops.in

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

abrt-install-ccpp-hook: abrt-install-ccpp-hook.in
	sed -e s,\@VAR_RUN\@,$(VAR_RUN),g \
	    -e s,\@libexecdir\@,$(libexecdir),g \
		$< >$@

abrt-harvest-vmcore: abrt_harvest_vmcore.py.in
	sed -e s,\@CONF_DIR\@,\$(CONF_DIR)\,g \
	    -e s,\@DEFAULT_DUMP_LOCATION\@,$(DEFAULT_DUMP_LOCATION),g \
		$< >$@

abrt-harvest-pstoreoops: abrt-harvest-pstoreoops.in
	sed -e s,\@CONF_DIR\@,\$(CONF_DIR)\,g \
	    -e s,\@DEFAULT_DUMP_LOCATION\@,$(DEFAULT_DUMP_LOCATION),g \
		$< >$@
