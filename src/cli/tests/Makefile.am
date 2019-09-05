SUBDIRS = clitests

TESTS = \
	test_filtering.py \
	test_match.py \
	test_utils.py
check_SCRIPTS = $(TESTS)

EXTRA_DIST = $(check_SCRIPTS)

if BUILD_PYTHON3
check-local:
	echo "Running tests with python3"; \
	exc=0; \
	for test in $(TESTS); do \
		python3 $$test || exc=1; \
	done; \
	exit $$exc
endif
