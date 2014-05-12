problem_PYTHON = __init__.py exception.py proxies.py tools.py watch.py config.py

problemdir = $(pythondir)/problem

pyabrtdir = $(problemdir)
pyabrt_LTLIBRARIES = _pyabrt.la

_pyabrt_la_SOURCES = \
    pyabrtmodule.c \
    pyabrt.c \
    common.h
_pyabrt_la_CPPFLAGS = \
    -I$(srcdir)/../../include \
    -D_GNU_SOURCE \
    $(PYTHON_CFLAGS) \
    $(LIBREPORT_CFLAGS) \
    -fPIE
_pyabrt_la_LDFLAGS = \
    -module \
    -avoid-version \
    -export-symbols-regex init_pyabrt \
     $(LIBREPORT_CFLAGS) \
    -Wl,-z,relro -Wl,-z,now
_pyabrt_la_LIBADD = \
    ../../lib/libabrt.la


config.py: config.py.in
	sed -e s,\@DEFAULT_DUMP_LOCATION\@,/var/spool/abrt,g \
		$< >$@

EXTRA_DIST = config.py.in

all-local: config.py
