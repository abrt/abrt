check_SCRIPTS = \
	test_api.py \
	test_get.py \
	test_internal.py \
	test_list.py \
	test_properties.py \
	test_tools.py

EXTRA_DIST = $(check_SCRIPTS) util.py base.py

if BUILD_PYTHON3
if HAVE_PYTEST
check-local: $(check_SCRIPTS)
	pytest-3 --capture=no --verbose $(check_SCRIPTS)
endif
endif
