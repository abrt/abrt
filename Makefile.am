SUBDIRS = lib src inc po icons

EXTRA_DIST = doc/CodingStyle abrt.spec

rpm: dist
	rpmbuild --define "_sourcedir `pwd`" \
				--define "_rpmdir `pwd`" \
				--define "_specdir `pwd`" \
				--define "_builddir `pwd`" \
				--define "_srcrpmdir `pwd`" \
				-ba abrt.spec
				
srpm: dist
	rpmbuild --define "_sourcedir `pwd`" \
				--define "_rpmdir `pwd`" \
				--define "_specdir `pwd`" \
				--define "_builddir `pwd`" \
				--define "_srcrpmdir `pwd`" \
				-bs abrt.spec
				
scratch-build: srpm
	koji build --scratch dist-f12 $(PACKAGE)-$(VERSION)-1.fc12.src.rpm
