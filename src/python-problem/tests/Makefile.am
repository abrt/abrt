TESTS = test_api.py test_get.py test_internal.py test_list.py test_properties.py test_tools.py
check_SCRIPTS = $(TESTS)

EXTRA_DIST = $(check_SCRIPTS) util.py base.py

if BUILD_PYTHON3
check-local:
	echo "Running tests with python3"; \
	exc=0; \
	for test in $(TESTS); do \
		python3 $$test || exc=1; \
	done; \
	exit $$exc
endif
