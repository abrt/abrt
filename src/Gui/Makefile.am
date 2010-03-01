#SUBDIRS=gui tui plugins tests conf

bin_SCRIPTS = abrt-gui

PYTHON_FILES = CCDBusBackend.py CCDumpList.py CCDump.py CC_gui_functions.py \
               CCReporterDialog.py abrt_utils.py \
               CCMainWindow.py CellRenderers.py ABRTExceptions.py \
               SettingsDialog.py ABRTPlugin.py PluginList.py PluginSettingsUI.py \
               PluginsSettingsDialog.py ConfBackend.py

GLADE_FILES = ccgui.glade report.glade settings.glade dialogs.glade settings_wizard.glade

EXTRA_DIST = $(PYTHON_FILES) $(GLADE_FILES) abrt-gui abrt.desktop abrt.png

#ccguidir = $(pkgdatadir)
applicationsdir = $(datadir)/applications
applications_DATA = abrt.desktop
pkgdata_PYTHON = $(PYTHON_FILES)
pkgdata_DATA = $(GLADE_FILES)
pixmapdir = $(datadir)/icons/hicolor/48x48/apps/
pixmap_DATA = abrt.png

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))

abrt.desktop:
	intltool-merge -d ../../po abrt.desktop.in abrt.desktop

install-exec-hook:
	for b in $(bin_SCRIPTS); do \
	    sed 's:/usr/share:$(datadir):g' -i $(DESTDIR)$(bindir)/$$b || exit $$?; \
	    sed 's:VERSION:@VERSION@:g' -i $(DESTDIR)$(bindir)/$$b || exit $$?; \
	done
