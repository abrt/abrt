ACLOCAL_AMFLAGS = -I m4
SUBDIRS = data src doc po icons tests apidoc

EXTRA_DIST = doc/coding-style abrt.spec.in abrt.pc.in \
	abrt-version asciidoc.conf init-scripts/*

dist_doc_DATA = README.md

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abrt.pc

systemdsystemunitdir = $(prefix)/lib/systemd/system
dist_systemdsystemunit_DATA = init-scripts/abrtd.service \
                              init-scripts/abrt-journal-core.service \
                              init-scripts/abrt-oops.service \
                              init-scripts/abrt-xorg.service \
                              init-scripts/abrt-pstoreoops.service \
                              init-scripts/abrt-upload-watch.service

if BUILD_ADDON_VMCORE
dist_systemdsystemunit_DATA += init-scripts/abrt-vmcore.service
endif

systemdtmpfilesdir = $(prefix)/lib/tmpfiles.d
systemdtmpfiles_DATA = init-scripts/abrt.conf

rpm:
	tito build --rpm

srpm:
	tito build --srpm

# value for overriding from command line
# e.g: make scratch-build DIST=f15
DIST=rawhide
scratch-build: srpm
	koji build --scratch $(DIST) `make srpm | grep Wrote | cut -d' ' -f2`

UPLOAD_URL ?= localhost

upload: dist
	scp $(distdir).tar.gz $$(test -n "$$UPLOAD_LOGIN" && echo "$$UPLOAD_LOGIN@")$(UPLOAD_URL)

.PHONY: release
release:
	tito tag
