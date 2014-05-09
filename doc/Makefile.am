-include ../config.mak

MAN1_TXT =
MAN1_TXT += abrt-action-analyze-c.txt
MAN1_TXT += abrt-action-trim-files.txt
MAN1_TXT += abrt-action-generate-backtrace.txt
MAN1_TXT += abrt-action-generate-core-backtrace.txt
MAN1_TXT += abrt-action-analyze-backtrace.txt
MAN1_TXT += abrt-action-analyze-oops.txt
MAN1_TXT += abrt-action-analyze-python.txt
MAN1_TXT += abrt-action-list-dsos.txt
MAN1_TXT += abrt-action-notify.txt
MAN1_TXT += abrt-auto-reporting.txt
MAN1_TXT += abrt-handle-upload.txt
MAN1_TXT += abrt-harvest-vmcore.txt
MAN1_TXT += abrt-server.txt
MAN1_TXT += abrt-cli.txt
MAN1_TXT += abrt-action-save-package-data.txt
MAN1_TXT += abrt-install-ccpp-hook.txt
MAN1_TXT += abrt-action-analyze-vmcore.txt

MAN5_TXT =
MAN5_TXT += abrt.conf.txt
MAN5_TXT += abrt-action-save-package-data.conf.txt
MAN5_TXT += abrt-oops.conf.txt

MAN8_TXT =
MAN8_TXT += abrtd.txt abrt-dbus.txt

# Manual pages are generated from .txt via Docbook
man1_MANS = ${MAN1_TXT:%.txt=%.1}
man5_MANS = ${MAN5_TXT:%.txt=%.5}
man8_MANS = ${MAN8_TXT:%.txt=%.8}


%.1 %.5 %.8: %.xml
	$(XMLTO_SILENT) xmlto man $< 2>&1 | sed '/Note/d'

%.xml: %.txt ../asciidoc.conf
	$(ASCIIDOC_SILENT) asciidoc --backend=docbook --doctype=manpage \
                           --conf-file ../asciidoc.conf \
                           -aabrt_version=$(PACKAGE_VERSION) -o $@ $<

EXTRA_DIST = $(MAN1_TXT) $(MAN5_TXT) $(MAN8_TXT)

CLEANFILES = $(man1_MANS) $(man5_MANS) $(man8_MANS)
