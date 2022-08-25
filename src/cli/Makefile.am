SUBDIRS = abrtcli tests

bin_SCRIPTS = abrt

profileconfigdir = $(sysconfdir)/profile.d
dist_profileconfig_DATA = abrt-console-notification.sh

TESTS_ENVIRONMENT = \
	PYTHONPATH="$(srcdir)":"$(top_srcdir)/src/python-problem":"$(top_builddir)/src/python-problem/problem/.libs"
TESTS = \
	tests/test_filtering.py \
	tests/test_match.py \
	tests/test_utils.py
dist_check_SCRIPTS = $(TESTS)

EXTRA_DIST = $(bin_SCRIPTS)
