ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src doc po icons tests apidoc

DISTCHECK_CONFIGURE_FLAGS = \
        --with-systemdsystemunitdir=$$dc_install_base/$(systemdsystemunitdir)

TESTSUITE_FILES =
TESTSUITE_FILES += examples/koops-tainted-g
TESTSUITE_FILES += examples/koops-tainted-insane
TESTSUITE_FILES += examples/koops-tainted-spaces
TESTSUITE_FILES += examples/cut_here.right
TESTSUITE_FILES += examples/oops-kernel-3.x.x
TESTSUITE_FILES += examples/oops1.right
TESTSUITE_FILES += examples/koops-tainted-bg1
TESTSUITE_FILES += examples/oops-same-as-oops4.right
TESTSUITE_FILES += examples/oops4.right
TESTSUITE_FILES += examples/oops-same-as-oops4.right
TESTSUITE_FILES += examples/hash-gen-oops6.right
TESTSUITE_FILES += examples/hash-gen-short-oops.right
TESTSUITE_FILES += examples/hash-gen-same-as-oops6.right
TESTSUITE_FILES += examples/oops-with-jiffies.test
TESTSUITE_FILES += examples/oops-with-jiffies.right
TESTSUITE_FILES += examples/oops_recursive_locking1.test
TESTSUITE_FILES += examples/oops_recursive_locking1.right
TESTSUITE_FILES += examples/nmi_oops.test
TESTSUITE_FILES += examples/nmi_oops.right
TESTSUITE_FILES += examples/nmi_oops_hash.test
TESTSUITE_FILES += examples/nmi_oops_hash.right
TESTSUITE_FILES += examples/oops10_s390x.test
TESTSUITE_FILES += examples/oops10_s390x.right
TESTSUITE_FILES += examples/oops_unsupported_hw.test
TESTSUITE_FILES += examples/oops_broken_bios.test


EXTRA_DIST = doc/coding-style abrt.spec.in abrt.pc.in \
	abrt-version asciidoc.conf init-scripts/* $(TESTSUITE_FILES) \
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
	koji build --scratch dist-$(DIST) `make srpm | grep Wrote | cut -d' ' -f2`

UPLOAD_URL ?= fedorahosted.org:abrt

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
	echo "* $$(date +'%a %b %d %Y') $$(git config --get user.name) <$$(git config --get user.email)> $$NEW_VER-1" > /tmp/changelog.tmp; \
	git log --oneline $$OLD_VER..HEAD | awk '{$$1=""; print "-" $$0} END {print ""}' | grep -v -e "- Merge" -e "- testsuite:" >> /tmp/changelog.tmp; \
	sed "$$(grep -n changelog abrt.spec.in | cut -f1 -d:)"'r /tmp/changelog.tmp' -i abrt.spec.in; \
	git add abrt.spec.in; \
	git commit -m "New version $$NEW_VER"; \
	git tag "$$NEW_VER"; \
	echo -n "$$NEW_VER" > abrt-version
	autoconf --force
	$(MAKE) upload
