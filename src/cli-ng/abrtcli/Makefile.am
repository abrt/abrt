PYFILES= \
	__init__.py \
	cli.py \
	config.py \
	filtering.py \
	l18n.py \
	match.py \
	utils.py

if BUILD_PYTHON2
abrtcli_PYTHON = $(PYFILES)
abrtclidir = $(pyexecdir)/abrtcli
endif

config.py: config.py.in
	sed -e s,\@LOCALE_DIR\@,$(localedir),g \
	-e s,\@VERSION\@,$(PACKAGE_VERSION),g \
	$< >$@

EXTRA_DIST = config.py.in

all-local: config.py
