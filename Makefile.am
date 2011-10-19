ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src doc po icons tests

DISTCHECK_CONFIGURE_FLAGS = \
        --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

TESTSUITE_FILES =
TESTSUITE_FILES += examples/koops-tainted-gw
TESTSUITE_FILES += examples/cut_here.right
TESTSUITE_FILES += examples/oops-kernel-3.x.x
TESTSUITE_FILES += examples/oops1.right
TESTSUITE_FILES += examples/koops-tainted-bg1
TESTSUITE_FILES += examples/oops-same-as-oops4.right

EXTRA_DIST = doc/coding-style abrt.spec.in abrt.pc.in \
	abrt-version asciidoc.conf init-scripts/* $(TESTSUITE_FILES)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abrt.pc

if HAVE_SYSTEMD
    dist_systemdsystemunit_DATA = init-scripts/abrtd.service \
                                  init-scripts/abrt-ccpp.service \
                                  init-scripts/abrt-oops.service \
                                  init-scripts/abrt-vmcore.service
else
    sysv_initdir = $(sysconfdir)/rc.d/init.d/
    sysv_init_SCRIPTS = init-scripts/abrtd init-scripts/abrt-ccpp init-scripts/abrt-oops \
                        init-scripts/abrt-vmcore
endif

RPM_DIRS = --define "_sourcedir `pwd`" \
	   --define "_rpmdir `pwd`" \
	   --define "_specdir `pwd`" \
	   --define "_builddir `pwd`" \
	   --define "_srcrpmdir `pwd`"

rpm:
	./gen-version
	autoconf --force
	$(MAKE) dist
	rpmbuild $(RPM_DIRS) -ba abrt.spec

srpm:
	./gen-version
	autoconf --force
	$(MAKE) dist
	rpmbuild $(RPM_DIRS) -bs abrt.spec

# value for overriding from command line
# e.g: make scratch-build DIST=f15
DIST=rawhide
scratch-build: srpm
	koji build --scratch dist-$(DIST) `make srpm | grep Wrote | cut -d' ' -f2`
