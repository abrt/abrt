NV="my_crash-0.0"

RPM_DIRS = --define "_sourcedir `pwd`" \
	--define "_rpmdir `pwd`" \
	--define "_specdir `pwd`" \
	--define "_builddir `pwd`" \
	--define "_srcrpmdir `pwd`"

tarball:
	echo "tarball"
	tar -czvf $(NV).tar.gz -C src/ .

rpm: tarball
	rpmbuild $(RPM_DIRS) -ba my_crash.spec

srpm: tarball
	rpmbuild $(RPM_DIRS) -bs my_crash.spec
