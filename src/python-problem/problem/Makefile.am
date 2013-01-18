problem_PYTHON = __init__.py exception.py proxies.py tools.py watch.py config.py

problemdir = $(pythondir)/problem

config.py: config.py.in
	sed -e s,\@DEFAULT_DUMP_LOCATION\@,$(DEFAULT_DUMP_LOCATION),g \
		$< >$@

EXTRA_DIST = config.py.in
