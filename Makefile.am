SUBDIRS = lib src inc po icons

EXTRA_DIST = doc/CodingStyle abrt.spec

rpm: dist
	rpmbuild --define "_sourcedir `pwd`" \
				--define "_rpmdir `pwd`" \
				--define "_specdir `pwd`" \
				--define "_builddir `pwd`" \
				--define "_srcrpmdir `pwd`" \
				-ba abrt.spec
