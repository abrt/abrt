bin_SCRIPTS = abrt-gui

PYTHON_FILES = CCDBusBackend.py CCDumpList.py CCDump.py CC_gui_functions.py \
               CCReporterDialog.py abrt_utils.py \
               CCMainWindow.py CellRenderers.py ABRTExceptions.py \
               SettingsDialog.py ABRTPlugin.py PluginList.py PluginSettingsUI.py \
               PluginsSettingsDialog.py ConfBackend.py CReporterAssistant.py

GLADE_FILES = ccgui.glade report.glade settings.glade dialogs.glade \
              settings_wizard.glade progress_window.glade

pkgdata_PYTHON = $(PYTHON_FILES)
pkgdata_DATA = $(GLADE_FILES)

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = abrt.desktop.in

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = $(PYTHON_FILES) $(GLADE_FILES) abrt-gui $(desktop_in_files)

CLEANFILES := $(notdir $(wildcard *~)) $(notdir $(wildcard *\#)) $(notdir $(wildcard \.\#*)) $(notdir $(wildcard *.pyc))

install-exec-hook:
	for b in $(bin_SCRIPTS); do \
	    sed 's:/usr/share:$(datadir):g' -i $(DESTDIR)$(bindir)/$$b || exit $$?; \
	    sed 's:VERSION:@VERSION@:g' -i $(DESTDIR)$(bindir)/$$b || exit $$?; \
	done

install-data-hook:
	sed 's:@VER@:$(VERSION):g' -i $(DESTDIR)$(pkgdatadir)/ccgui.glade || exit $$?
