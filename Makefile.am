SUBDIRS = lib src inc po icons

EXTRA_DIST = doc/CodingStyle abrt.spec

rpm: dist
	rpmbuild --define "_sourcedir `pwd`" \
				--define "_rpmdir `pwd`" \
				--define "_specdir `pwd`" \
				--define "_builddir `pwd`" \
				--define "_srcrpmdir `pwd`" \
				--define "dist .el6" \
				-ba abrt.spec
				
srpm: dist
	rpmbuild --define "_sourcedir `pwd`" \
				--define "_rpmdir `pwd`" \
				--define "_specdir `pwd`" \
				--define "_builddir `pwd`" \
				--define "_srcrpmdir `pwd`" \
				--define "dist .el6" \
				-bs abrt.spec
				
scratch-build: srpm
	brew build --scratch RHEL-6-build $(PACKAGE)-$(VERSION)-1.el6.src.rpm
