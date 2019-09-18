confdir = $(CONF_DIR)
pluginsconfdir = $(PLUGINS_CONF_DIR)

dist_pluginsconf_DATA = \
    CCpp.conf \
    oops.conf

sbin_SCRIPTS = \
    abrt-harvest-pstoreoops

bin_PROGRAMS = \
    abrt-merge-pstoreoops

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

EXTRA_DIST = \
    abrt-harvest-pstoreoops.in

ALL_DEPENDENCES =

if BUILD_PYTHON3
py3hookdir = $(pyexecdir)
dist_pluginsconf_DATA += python3.conf
ALL_DEPENDENCES += abrt_exception_handler3.py

py3hook_PYTHON = \
	abrt_exception_handler3.py \
	abrt3.pth \
	abrt_exception_handler3_container.py \
	abrt3_container.pth

EXTRA_DIST += \
    abrt_exception_handler3.py.in
endif

if BUILD_ADDON_VMCORE
sbin_SCRIPTS += \
    abrt-harvest-vmcore
dist_pluginsconf_DATA += \
    vmcore.conf
EXTRA_DIST +=  \
	abrt_harvest_vmcore.py.in
endif

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc)) $(man1_MANS)

# Generate on build
all: $(ALL_DEPENDENCES)

# Must be synchronized with another sed call below.
abrt_exception_handler.py: abrt_exception_handler.py.in
	sed -e s,\@VAR_RUN\@,\"$(VAR_RUN)\",g \
	    -e s,\@CONF_DIR\@,\"$(CONF_DIR)\",g \
	    abrt_exception_handler.py.in >abrt_exception_handler.py

abrt_exception_handler3.py: abrt_exception_handler3.py.in
	sed -e s,\@VAR_RUN\@,\"$(VAR_RUN)\",g \
	    -e s,\@CONF_DIR\@,\"$(CONF_DIR)\",g \
	    abrt_exception_handler3.py.in >abrt_exception_handler3.py

# RPM fix: we need to regenerate abrt_exception_handler.py, because it has the default ddir
install-data-local:
if BUILD_PYTHON3
	sed -e s,\@VAR_RUN\@,\"$(VAR_RUN)\",g \
	    -e s,\@CONF_DIR\@,\"$(CONF_DIR)\",g \
	    abrt_exception_handler3.py.in >abrt_exception_handler3.py
endif

abrt-harvest-vmcore: abrt_harvest_vmcore.py.in
	sed -e s,\@CONF_DIR\@,\$(CONF_DIR)\,g \
	    -e s,\@DEFAULT_DUMP_LOCATION\@,$(DEFAULT_DUMP_LOCATION),g \
	    -e s,\@FINDMNT\@,$(FINDMNT),g \
		$< >$@

abrt-harvest-pstoreoops: abrt-harvest-pstoreoops.in
	sed -e s,\@CONF_DIR\@,\$(CONF_DIR)\,g \
	    -e s,\@DEFAULT_DUMP_LOCATION\@,$(DEFAULT_DUMP_LOCATION),g \
		$< >$@
