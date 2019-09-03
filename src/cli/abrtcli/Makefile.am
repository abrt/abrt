PYFILES= \
	__init__.py \
	cli.py \
	config.py \
	filtering.py \
	i18n.py \
	match.py \
	utils.py

abrtcli_PYTHON = $(PYFILES)
abrtclidir = $(pyexecdir)/abrtcli

config.py: config.py.in
	sed -e s,\@LOCALE_DIR\@,$(localedir),g \
	-e s,\@VERSION\@,$(PACKAGE_VERSION),g \
	$< >$@

EXTRA_DIST = config.py.in

all-local: config.py
