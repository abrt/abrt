SUBDIRS = lib src inc po icons

EXTRA_DIST = doc/CodingStyle abrt.spec

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
	rpmbuild --define "_buildid `date +%G%m%e%k%M%S`" \
		$(RPM_DIRS) -ba abrt.spec

test-srpm: dist
	rpmbuild --define "_buildid `date +%G%m%e%k%M%S`" \
		$(RPM_DIRS) -bs abrt.spec

scratch-build: srpm
	koji build --scratch dist-f12 $(PACKAGE)-$(VERSION)-1.fc12.src.rpm
