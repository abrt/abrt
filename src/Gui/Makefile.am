#SUBDIRS=gui tui plugins tests conf

bin_SCRIPTS = cc-gui

PYTHON_FILES = CCDBusBackend.py  CCDumpList.py  CCDump.py  CC_gui_functions.py  ccgui.glade  CCReporterDialog.py CCMainWindow.py

GLADE_FILES = ccgui.glade

EXTRA_DIST = $(PYTHON_FILES) $(GLADE_FILES) cc-gui

#ccguidir = $(pkgdatadir)
pkgdata_PYTHON = $(PYTHON_FILES)
pkgdata_DATA = $(GLADE_FILES)

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))