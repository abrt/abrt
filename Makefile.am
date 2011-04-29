ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src po icons tests doc

DISTCHECK_CONFIGURE_FLAGS = \
        --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

EXTRA_DIST = doc/coding-style abrt.spec.in abrt.pc.in doc/abrt-plugin/ doc/howto-write-reporter \
	gen-version

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abrt.pc

RPM_DIRS = --define "_sourcedir `pwd`" \
	   --define "_rpmdir `pwd`" \
	   --define "_specdir `pwd`" \
	   --define "_builddir `pwd`" \
	   --define "_srcrpmdir `pwd`"

rpm:
	autoconf --force
	./gen-version
	$(MAKE) dist
	rpmbuild $(RPM_DIRS) -ba abrt.spec

srpm:
	autoconf --force
	./gen-version
	$(MAKE) dist
	rpmbuild $(RPM_DIRS) -bs abrt.spec

# value for overriding from command line
# e.g: make scratch-build DIST=f15
DIST=rawhide
scratch-build: srpm
	koji build --scratch dist-$(DIST) `make srpm | grep Wrote | cut -d' ' -f2`
