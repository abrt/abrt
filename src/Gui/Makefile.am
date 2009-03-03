#SUBDIRS=gui tui plugins tests conf

bin_SCRIPTS = abrt-gui

PYTHON_FILES = CCDBusBackend.py  CCDumpList.py  CCDump.py  CC_gui_functions.py  ccgui.glade report.glade CCReporterDialog.py CCReport.py CCMainWindow.py exception.py

GLADE_FILES = ccgui.glade report.glade

EXTRA_DIST = $(PYTHON_FILES) $(GLADE_FILES) abrt-gui abrt.desktop

#ccguidir = $(pkgdatadir)
pkgdata_PYTHON = $(PYTHON_FILES)
pkgdata_DATA = $(GLADE_FILES)

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))