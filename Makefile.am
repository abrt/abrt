ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src doc po icons tests apidoc

DISTCHECK_CONFIGURE_FLAGS = \
        --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

EXTRA_DIST = doc/coding-style abrt.spec.in abrt.pc.in \
	abrt-version asciidoc.conf init-scripts/* \
	augeas/test_abrt.aug

dist_doc_DATA = README.md

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = abrt.pc

augeasdir = $(AUGEAS_LENS_LIB_DIR)
dist_augeas_DATA = augeas/abrt.aug

check-local:
	$(AUGPARSE) -I $(top_builddir)/augeas $(top_builddir)/augeas/test_abrt.aug

if HAVE_SYSTEMD
    dist_systemdsystemunit_DATA = init-scripts/abrtd.service \
                                  init-scripts/abrt-ccpp.service \
                                  init-scripts/abrt-journal-core.service \
                                  init-scripts/abrt-oops.service \
                                  init-scripts/abrt-xorg.service \
                                  init-scripts/abrt-pstoreoops.service \
                                  init-scripts/abrt-upload-watch.service

if BUILD_ADDON_VMCORE
    dist_systemdsystemunit_DATA += init-scripts/abrt-vmcore.service
endif

if BUILD_ATOMIC
    dist_systemdsystemunit_DATA += init-scripts/abrt-coredump-helper.service
endif

    systemdtmpfilesdir = $(prefix)/lib/tmpfiles.d
    systemdtmpfiles_DATA = init-scripts/abrt.conf
else
    sysv_initdir = $(sysconfdir)/rc.d/init.d/
    sysv_init_SCRIPTS = init-scripts/abrtd \
                        init-scripts/abrt-ccpp \
                        init-scripts/abrt-oops \
                        init-scripts/abrt-xorg \
                        init-scripts/abrt-vmcore \
                        init-scripts/abrt-pstoreoops \
                        init-scripts/abrt-upload-watch

if BUILD_ADDON_VMCORE
    sysv_init_SCRIPTS += init-scripts/abrt-vmcore
endif

endif

RPM_DIRS = --define "_sourcedir `pwd`" \
	   --define "_rpmdir `pwd`/build" \
	   --define "_specdir `pwd`" \
	   --define "_builddir `pwd`/build" \
	   --define "_srcrpmdir `pwd`/build"

rpm:
	./gen-version
	autoconf --force
	$(MAKE) dist
	rpmbuild $(RPM_DIRS) $(RPM_FLAGS) -ba abrt.spec

srpm:
	./gen-version
	autoconf --force
	$(MAKE) dist
	rpmbuild $(RPM_DIRS) -bs abrt.spec

# value for overriding from command line
# e.g: make scratch-build DIST=f15
DIST=rawhide
scratch-build: srpm
	koji build --scratch $(DIST) `make srpm | grep Wrote | cut -d' ' -f2`

UPLOAD_URL ?= localhost

upload: dist
	scp $(distdir).tar.gz $$(test -n "$$UPLOAD_LOGIN" && echo "$$UPLOAD_LOGIN@")$(UPLOAD_URL)

.PHONY: release-fix
release-fix:
	OLD_VER=$$(git describe --tags --match "[0-9]*" --abbrev=0 HEAD 2>/dev/null); \
	MAJOR_VER=$$(echo $$OLD_VER | cut -d. -f 1); \
	MINOR_VER=$$(echo $$OLD_VER | cut -d. -f 2); \
	FIX_VER=$$(echo $$OLD_VER | cut -d. -f 3); \
	$(MAKE) release OLD_VER=$$OLD_VER NEW_VER="$$MAJOR_VER.$$MINOR_VER.$$((FIX_VER+1))"

.PHONY: release-minor
release-minor:
	OLD_VER=$$(git describe --tags --match "[0-9]*" --abbrev=0 HEAD 2>/dev/null); \
	MAJOR_VER=$$(echo $$OLD_VER | cut -d. -f 1); \
	MINOR_VER=$$(echo $$OLD_VER | cut -d. -f 2); \
	$(MAKE) release OLD_VER=$$OLD_VER NEW_VER="$$MAJOR_VER.$$((MINOR_VER+1)).0"

.PHONY: release-major
release-major:
	OLD_VER=$$(git describe --tags --match "[0-9]*" --abbrev=0 HEAD 2>/dev/null); \
	MAJOR_VER=$$(echo $$OLD_VER | cut -d. -f 1); \
	$(MAKE) release OLD_VER=$$OLD_VER NEW_VER="$$((MAJOR_VER+1)).0.0"

.PHONY: release
release:
	maint/pull-translations || exit 1; \
	echo "* $$(LANG='en_US.UTF-8' date +'%a %b %d %Y') $$(git config --get user.name) <$$(git config --get user.email)> $$NEW_VER-1" | sort > /tmp/changelog.tmp; \
	git log --oneline $$OLD_VER..HEAD | awk '{$$1=""; if (a[$$0]++ == 0) print "-" $$0} END {print ""}' | grep -v -e "- Merge" -e "- testsuite:" -e "- make:" >> /tmp/changelog.tmp; \
	sed "$$(grep -n changelog abrt.spec.in | cut -f1 -d:)"'r /tmp/changelog.tmp' -i abrt.spec.in; \
	sed -e "s/^## \[Unreleased\]/## [Unreleased]\n\n## [$$NEW_VER]/" \
	    -e "s/^\[Unreleased\]: \(https:\/\/.*\/compare\)\(\/.*\)\.\.\.HEAD/[Unreleased]: \1\/$$NEW_VER...HEAD\n[$$NEW_VER]: \1\2...$$NEW_VER/" \
	    -i CHANGELOG.md; \
	sed 's|DEF_VER=.*$$|DEF_VER='$$NEW_VER'|' -i gen-version; \
	git add gen-version abrt.spec.in CHANGELOG.md; \
	git commit -m "New version $$NEW_VER"; \
	git tag "$$NEW_VER"; \
	echo -n "$$NEW_VER" > abrt-version
	autoconf --force
