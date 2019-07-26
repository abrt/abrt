PYFILES= __init__.py exception.py proxies.py tools.py watch.py config.py

PYEXTFILES = pyabrtmodule.c pyabrt.c common.h
PYEXTCPPFLAGS = \
    -I$(srcdir)/../../include \
    -D_GNU_SOURCE \
    $(LIBREPORT_CFLAGS) \
    -fPIE

PYEXTLDFLAGS = \
    -module \
    -avoid-version \
     $(LIBREPORT_CFLAGS) \
    -Wl,-z,relro -Wl,-z,now

if BUILD_PYTHON3
problem3dir = $(pyexecdir)/problem
py3abrtdir = $(problem3dir)

problem3_PYTHON = $(PYFILES)
py3abrt_LTLIBRARIES = _py3abrt.la

_py3abrt_la_SOURCES = $(PYEXTFILES)
_py3abrt_la_CPPFLAGS = \
    $(PYEXTCPPFLAGS) \
    $(PYTHON3_CFLAGS)
_py3abrt_la_LDFLAGS = \
    $(PYEXTLDFLAGS) \
    -export-symbols-regex PyInit__py3abrt
_py3abrt_la_LIBADD = \
    ../../lib/libabrt.la \
    $(PYTHON3_LIBS)
endif

config.py: config.py.in
	sed -e s,\@DEFAULT_DUMP_LOCATION\@,$(DEFAULT_DUMP_LOCATION),g \
		-e s,\@HAVE_POLKIT\@,$(HAVE_POLKIT),g \
		$< >$@

EXTRA_DIST = config.py.in

all-local: config.py
