SUBDIRS = problems-service

-include ../config.mak

MAN1_TXT =
MAN1_TXT += abrt.txt
MAN1_TXT += abrt-action-analyze-c.txt
MAN1_TXT += abrt-action-trim-files.txt
MAN1_TXT += abrt-action-generate-backtrace.txt
MAN1_TXT += abrt-action-generate-core-backtrace.txt
MAN1_TXT += abrt-action-analyze-backtrace.txt
MAN1_TXT += abrt-action-analyze-oops.txt
MAN1_TXT += abrt-action-analyze-python.txt
MAN1_TXT += abrt-action-analyze-xorg.txt
MAN1_TXT += abrt-action-analyze-vulnerability.txt
MAN1_TXT += abrt-action-list-dsos.txt
MAN1_TXT += abrt-action-perform-ccpp-analysis.txt
MAN1_TXT += abrt-action-notify.txt
MAN1_TXT += abrt-applet.txt
MAN1_TXT += abrt-dump-oops.txt
MAN1_TXT += abrt-dump-journal-core.txt
MAN1_TXT += abrt-dump-journal-oops.txt
MAN1_TXT += abrt-dump-journal-xorg.txt
MAN1_TXT += abrt-dump-xorg.txt
MAN1_TXT += abrt-auto-reporting.txt
if BUILD_RETRACE_CLIENT
MAN1_TXT += abrt-retrace-client.txt
endif
MAN1_TXT += abrt-handle-upload.txt
MAN1_TXT += abrt-harvest-pstoreoops.txt
MAN1_TXT += abrt-merge-pstoreoops.txt
MAN1_TXT += abrt-server.txt
MAN1_TXT += abrt-action-save-package-data.txt
MAN1_TXT += abrt-action-analyze-ccpp-local.txt
MAN1_TXT += abrt-watch-log.txt
MAN1_TXT += abrt-upload-watch.txt
MAN1_TXT += system-config-abrt.txt
if BUILD_BODHI
MAN1_TXT += abrt-bodhi.txt
MAN1_TXT += abrt-action-find-bodhi-update.txt
endif


MAN5_TXT =
MAN5_TXT += abrt.conf.txt
MAN5_TXT += abrt-action-save-package-data.conf.txt
MAN5_TXT += abrt-xorg.conf.txt
if BUILD_PYTHON3
MAN5_TXT += python3-abrt.conf.txt
endif
MAN5_TXT += abrt-CCpp.conf.txt
MAN5_TXT += abrt-oops.conf.txt
MAN5_TXT += gpg_keys.conf.txt

MAN5_PREFORMATTED =
MAN5_PREFORMATTED += abrt_event.conf.5
MAN5_PREFORMATTED += ccpp_event.conf.5
if BUILD_RETRACE_CLIENT
MAN5_PREFORMATTED += ccpp_retrace_event.conf.5
endif
MAN5_PREFORMATTED += gconf_event.conf.5
MAN5_PREFORMATTED += koops_event.conf.5
if BUILD_PYTHON3
MAN5_PREFORMATTED += python3_event.conf.5
endif
MAN5_PREFORMATTED += smart_event.conf.5
MAN5_PREFORMATTED += vimrc_event.conf.5
MAN5_PREFORMATTED += xorg_event.conf.5

if BUILD_ADDON_VMCORE
MAN1_TXT += abrt-harvest-vmcore.txt
MAN1_TXT += abrt-action-analyze-vmcore.txt
MAN1_TXT += abrt-action-check-oops-for-hw-error.txt
MAN5_TXT += abrt-vmcore.conf.txt
MAN5_PREFORMATTED += vmcore_event.conf.5
endif

MAN8_TXT =
MAN8_TXT += abrtd.txt abrt-dbus.txt

# Manual pages are generated from .txt via Docbook
man1_MANS = ${MAN1_TXT:%.txt=%.1}
man5_MANS = ${MAN5_TXT:%.txt=%.5} ${MAN5_PREFORMATTED}
man8_MANS = ${MAN8_TXT:%.txt=%.8}

abrt-CCpp.conf.txt: abrt-CCpp.conf.txt.in
	sed -e s,\@DEFAULT_PACKAGE_MANAGER\@,$(DEFAULT_PACKAGE_MANAGER),g \
	    $< > $@

%.1 %.5 %.8: %.xml
	$(XMLTO_SILENT) xmlto man $< 2>&1 | sed '/Note/d'

%.xml: %.txt ../asciidoc.conf
	$(ASCIIDOC_SILENT) asciidoc --backend=docbook --doctype=manpage \
                           --conf-file ../asciidoc.conf \
                           -aabrt_version=$(PACKAGE_VERSION) -o $@ $<

EXTRA_DIST = $(MAN1_TXT) $(MAN5_TXT) $(MAN8_TXT) $(MAN5_PREFORMATTED)
EXTRA_DIST += abrt-CCpp.conf.txt.in

CLEANFILES = ${MAN1_TXT:%.txt=%.1} ${MAN5_TXT:%.txt=%.5} ${MAN8_TXT:%.txt=%.8}
