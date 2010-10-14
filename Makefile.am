ACLOCAL_AMFLAGS = -I m4
SUBDIRS = lib src inc po icons

DISTCHECK_CONFIGURE_FLAGS = \
        --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

EXTRA_DIST = doc/coding-style abrt.spec abrt.pc.in doc/abrt-plugin/ doc/howto-write-reporter

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abrt.pc

RPM_DIRS = --define "_sourcedir `pwd`" \
	   --define "_rpmdir `pwd`" \
	   --define "_specdir `pwd`" \
	   --define "_builddir `pwd`" \
	   --define "_srcrpmdir `pwd`"

rpm: dist
	rpmbuild $(RPM_DIRS) -ba abrt.spec

srpm: dist
	rpmbuild $(RPM_DIRS) -bs abrt.spec

test-rpm: dist
	rpmbuild --define "_buildid `date +%G%m%d%H%M%S`" \
		$(RPM_DIRS) -ba abrt.spec

test-srpm: dist
	rpmbuild --define "_buildid `date +%G%m%d%H%M%S`" \
		$(RPM_DIRS) -bs abrt.spec

scratch-build: srpm
	koji build --scratch dist-f12 $(PACKAGE)-$(VERSION)-1.fc12.src.rpm
