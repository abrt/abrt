#SUBDIRS=gui tui plugins tests conf

#bin_SCRIPTS = mainwindow.py

PYTHON_FILES = CCDBusBackend.py  CCDumpList.py  CCDump.py  CC_gui_functions.py  ccgui.glade  CCReporterDialog.py

GLADE_FILES = ccgui.glade

EXTRA_DIST = $(PYTHON_FILES) $(GLADE_FILES)

ccguidir = $(pkgdatadir)
ccgui_PYTHON = $(PYTHON_FILES)
ccgui_DATA = $(GLADE_FILES)

execdir = $(sbindir)

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))

check-local:
	@for i in $(notdir $(wildcard *.py)); do echo $(PYTHON_FILES)|grep -q $$i || (echo $$i is missing;false) || exit 1;done && echo "Source file check PASSED"
install-exec-local:
	mkdir -p $(DESTDIR)$(pkgdatadir) $(DESTDIR)$(sbindir) $(DESTDIR)$(bindir)
	install -m 0755 $(srcdir)/mainwindow.py $(DESTDIR)$(pkgdatadir)
	ln -fs $(pkgdatadir)/mainwindow.py $(DESTDIR)$(bindir)/crash-catcher-gui
uninstall-local:
	rm -fr $(DESTDIR)$(pkgdatadir)
	rm -f  $(DESTDIR)$(bindir)/crash-catcher-gui
                                
                                                                                                                        