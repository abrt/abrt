#SUBDIRS=gui tui plugins tests conf

bin_SCRIPTS = abrt-gui

PYTHON_FILES = CCDBusBackend.py  CCDumpList.py  CCDump.py  CC_gui_functions.py \
               CCReporterDialog.py CCReport.py abrt_utils.py \
               CCMainWindow.py CellRenderers.py ABRTExceptions.py \
               SettingsDialog.py ABRTPlugin.py PluginList.py PluginSettingsUI.py \
               PluginsSettingsDialog.py ConfBackend.py

GLADE_FILES = ccgui.glade report.glade settings.glade dialogs.glade

EXTRA_DIST = $(PYTHON_FILES) $(GLADE_FILES) abrt-gui abrt.desktop

#ccguidir = $(pkgdatadir)
pkgdata_PYTHON = $(PYTHON_FILES)
pkgdata_DATA = $(GLADE_FILES)

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))

install-exec-hook:
	for b in $(bin_SCRIPTS); do \
	    sed 's:/usr/share:$(datadir):g' -i $(DESTDIR)$(bindir)/$$b || exit $$?; \
	done
