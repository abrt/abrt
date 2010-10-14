SUBDIRS = lib tests

bin_PROGRAMS = btparser
btparser_CFLAGS = -Wall -Werror
btparser_SOURCES = btparser.c
btparser_LDADD = lib/libbtparser.la

man_MANS = btparser.1
EXTRA_DIST = $(man_MANS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = btparser.pc
DISTCLEANFILES = $(pkgconfig_DATA)
EXTRA_DIST += btparser.pc.in

EXTRA_DIST += autogen.sh

UPLOAD_URI = klic.name:/home/karel/public_html/ftp.klic.name/public/btparser
.PHONY: upload
upload:
	make dist-gzip
	scp $(distdir).tar.gz $(UPLOAD_URI)
	make dist-zip
	scp $(distdir).zip $(UPLOAD_URI)
	make dist-xz
	scp $(distdir).tar.xz $(UPLOAD_URI)

RPM_DIRS = --define "_sourcedir `pwd`" \
           --define "_rpmdir `pwd`" \
           --define "_specdir `pwd`" \
           --define "_builddir `pwd`" \
           --define "_srcrpmdir `pwd`"

EXTRA_DIST += btparser.spec.in
rpm: dist-xz btparser.spec
	rpmbuild $(RPM_DIRS) -ba btparser.spec
srpm: dist-xz btparser.spec
	rpmbuild $(RPM_DIRS) -bs btparser.spec
