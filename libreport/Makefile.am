ACLOCAL_AMFLAGS = -I m4
SUBDIRS = po src

EXTRA_DIST = libreport.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libreport.pc

RPM_DIRS = --define "_sourcedir `pwd`" \
	   --define "_rpmdir `pwd`" \
	   --define "_specdir `pwd`" \
	   --define "_builddir `pwd`" \
	   --define "_srcrpmdir `pwd`"

rpm:
	autoconf --force
	$(MAKE) dist
	rpmbuild $(RPM_DIRS) -ba libreport.spec

srpm:
	autoconf --force
	$(MAKE) dist
	rpmbuild $(RPM_DIRS) -bs libreport.spec

# value for overriding from command line
# e.g: make scratch-build DIST=f15
DIST=rawhide
scratch-build: srpm
	koji build --scratch dist-$(DIST) `make srpm | grep Wrote | cut -d' ' -f2`
