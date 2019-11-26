augeasdir = $(datadir)/augeas/lenses
dist_augeas_DATA = abrt.aug

check-local:
	$(AUGPARSE) --include="$(abs_srcdir)" "$(abs_srcdir)/test_abrt.aug"

EXTRA_DIST = test_abrt.aug
