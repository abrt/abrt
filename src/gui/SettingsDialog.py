import sys
import gtk
from PluginList import getPluginInfoList
from CC_gui_functions import *
#from PluginSettingsUI import PluginSettingsUI
from abrt_utils import _, log, log1, log2


#FIXME: create a better struct, to automatize hydrate/dehydrate process
settings_dict = { "Common":
                    {"OpenGPGCheck":bool,
                     "Database":object,
                     "EnabledPlugins": list,
                     "BlackList": list,
                     "MaxCrashReportsSize": int,
                     "OpenGPGPublicKeys": list,
                     },
                }

class SettingsDialog:
    def __init__(self, parent, daemon):
        builderfile = "%s%ssettings.glade" % (sys.path[0],"/")
        self.ccdaemon = daemon
        self.builder = gtk.Builder()
        self.builder.add_from_file(builderfile)
        self.window = self.builder.get_object("wGlobalSettings")
        self.builder.get_object("bSaveSettings").connect("clicked", self.on_ok_clicked)
        self.builder.get_object("bCancelSettings").connect("clicked", self.on_cancel_clicked)
        self.builder.get_object("bAddCronJob").connect("clicked", self.on_bAddCronJob_clicked)

        # action plugin list for Cron tab
        self.actionPluginsListStore = gtk.ListStore(str, object)
        self.actionPluginsListStore.append([_("<b>Select plugin</b>"), None])
        # database plugin list
        self.databasePluginsListStore = gtk.ListStore(str, object)
        self.databasePluginsListStore.append([_("<b>Select database backend</b>"), None])

        self.dbcombo = self.builder.get_object("cbDatabase")
        self.dbcombo.set_model(self.databasePluginsListStore)
        cell = gtk.CellRendererText()
        self.dbcombo.pack_start(cell)
        self.dbcombo.add_attribute(cell, "markup", 0)
        # blacklist edit
        self.builder.get_object("bEditBlackList").connect("clicked", self.on_blacklistEdit_clicked)

        self.builder.get_object("bOpenGPGPublicKeys").connect("clicked", self.on_GPGKeysEdit_clicked)
        self.builder.get_object("bAddAction").connect("clicked", self.on_bAddAction_clicked)
        # AnalyzerActionsAndReporters
        self.analyzerPluginsListStore = gtk.ListStore(str, object)
        self.analyzerPluginsListStore.append([_("<b>Select plugin</b>"), None])
        # GPG keys
        self.wGPGKeys = self.builder.get_object("wGPGKeys")
        self.GPGKeysListStore = gtk.ListStore(str)
        self.tvGPGKeys = self.builder.get_object("tvGPGKeys")
        self.tvGPGKeys.set_model(self.GPGKeysListStore)
        self.builder.get_object("bCancelGPGKeys").connect("clicked", self.on_bCancelGPGKeys_clicked)
        self.builder.get_object("bSaveGPGKeys").connect("clicked", self.on_bSaveGPGKeys_clicked)

        gpg_column = gtk.TreeViewColumn()
        cell = gtk.CellRendererText()
        gpg_column.pack_start(cell)
        gpg_column.add_attribute(cell, "text", 0)
        self.tvGPGKeys.append_column(gpg_column)

    def filter_settings(self, model, miter, data):
        return True

    def hydrate(self):
        try:
            self.settings = self.ccdaemon.getSettings()
        except Exception, e:
            # FIXME: this should be error gui message!
            print e
        try:
            self.pluginlist = getPluginInfoList(self.ccdaemon, refresh=True)
        except Exception, e:
            raise Exception("Comunication with daemon has failed, have you restarted the daemon after update?")

        ## hydrate cron jobs:
        for key,val in self.settings["Cron"].iteritems():
            # actions are separated by ','
            actions = val.split(',')
            self.settings["Cron"][key] = actions
        for plugin in self.pluginlist.getActionPlugins():
            it = self.actionPluginsListStore.append([plugin.getName(), plugin])
            for key,val in self.settings["Cron"].iteritems():
                if plugin.getName() in val:
                    cron_job = (key,it)
                    self.add_CronJob(cron_job)
                    self.settings["Cron"][key].remove(plugin.getName())
        # hydrate common
        common = self.settings["Common"]
        # ensure that all expected keys exist:
        if "OpenGPGCheck" not in common:
            common["OpenGPGCheck"] = "no" # check unsigned pkgs too
        ## gpgcheck
        self.builder.get_object("cbOpenGPGCheck").set_active(common["OpenGPGCheck"] == 'yes')
        ## database
        for dbplugin in self.pluginlist.getDatabasePlugins():
            it = self.databasePluginsListStore.append([dbplugin.getName(), dbplugin])
            if common["Database"] == dbplugin.getName():
                self.dbcombo.set_active_iter(it)
        ## MaxCrashSize
        self.builder.get_object("sbMaxCrashReportsSize").set_value(float(common["MaxCrashReportsSize"]))
        ## GPG keys
        try:
            self.builder.get_object("eOpenGPGPublicKeys").set_text(common["OpenGPGPublicKeys"])
            self.gpgkeys = common["OpenGPGPublicKeys"].split(',')
            for gpgkey in self.gpgkeys:
                self.GPGKeysListStore.append([gpgkey])
        except:
            pass

        ## blacklist
        self.builder.get_object("eBlacklist").set_text(common["BlackList"])
        # hydrate AnalyzerActionsAndReporters
        AnalyzerActionsAndReporters = self.settings["AnalyzerActionsAndReporters"]
        for analplugin in self.pluginlist.getAnalyzerPlugins():
            it = self.analyzerPluginsListStore.append([analplugin.getName(), analplugin])
            if analplugin.getName() in AnalyzerActionsAndReporters:
                action = (AnalyzerActionsAndReporters[analplugin.getName()], it)
                self.add_AnalyzerAction(action)

    def on_bCancelGPGKeys_clicked(self, button):
        self.wGPGKeys.hide()

    def on_bSaveGPGKeys_clicked(self, button):
        self.wGPGKeys.hide()

    def on_bAddGPGKey_clicked(self, button):
        print "add GPG key"

    def on_bRemoveGPGKey_clicked(self, button):
        print "add GPG key"

    def on_blacklistEdit_clicked(self, button):
        print "edit blacklist"

    def on_GPGKeysEdit_clicked(self, button):
        self.wGPGKeys.show()

    def on_ok_clicked(self, button):
        self.dehydrate()
        self.window.hide()

    def on_cancel_clicked(self, button):
        self.window.hide()

    def on_remove_CronJob_clicked(self, button, job_hbox):
        self.removeHBoxWihtChildren(job_hbox)

    def on_remove_Action_clicked(self, button, binding_hbox):
        self.removeHBoxWihtChildren(binding_hbox)

    def removeHBoxWihtChildren(self, job_hbox):
        job_hbox.get_parent().remove(job_hbox)
        for child in job_hbox.get_children():
            child.destroy()
        job_hbox.destroy()

    def add_CronJob(self, job=None):
        hbox = gtk.HBox()
        hbox.set_spacing(6)
        time = gtk.Entry()
        remove_image = gtk.Image()
        remove_image.set_from_stock("gtk-remove",gtk.ICON_SIZE_MENU)
        remove_button = gtk.Button()
        remove_button.set_image(remove_image)
        remove_button.set_tooltip_text(_("Remove this job"))
        remove_button.connect("clicked", self.on_remove_CronJob_clicked, hbox)
        plugins = gtk.ComboBox()
        cell = gtk.CellRendererText()

        plugins.pack_start(cell)
        plugins.add_attribute(cell, 'markup', 0)
        plugins.set_model(self.actionPluginsListStore)

        if job:
            time.set_text(job[0])
            plugins.set_active_iter(job[1])
        else:
            plugins.set_active(0)
        hbox.pack_start(plugins,True)
        hbox.pack_start(time,True)
        hbox.pack_start(remove_button,False)
        self.builder.get_object("vbCronJobs").pack_start(hbox,False)

        hbox.show_all()

    def on_bAddCronJob_clicked(self, button):
        self.add_CronJob()
        print "add"

    def on_bEditAction_clicked(self, button, data=None):
        print "edit action"

    def add_AnalyzerAction(self, action=None):
        #print "add_AnalyzerAction"
        hbox = gtk.HBox()
        hbox.set_spacing(6)
        action_list = gtk.Entry()
        edit_actions = gtk.Button()
        edit_actions.set_tooltip_text("Edit actions")
        edit_image = gtk.Image()
        edit_image.set_from_stock("gtk-edit", gtk.ICON_SIZE_MENU)
        edit_actions.set_image(edit_image)
        edit_actions.connect("clicked", self.on_bEditAction_clicked)

        remove_image = gtk.Image()
        remove_image.set_from_stock("gtk-remove",gtk.ICON_SIZE_MENU)
        remove_button = gtk.Button()
        remove_button.set_image(remove_image)
        remove_button.set_tooltip_text(_("Remove this action"))
        remove_button.connect("clicked", self.on_remove_Action_clicked, hbox)

        reporters = gtk.ComboBox()
        cell = gtk.CellRendererText()
        reporters.pack_start(cell)
        reporters.add_attribute(cell, 'markup', 0)
        reporters.set_model(self.analyzerPluginsListStore)

        if action:
            action_list.set_text(action[0])
            reporters.set_active_iter(action[1])
        else:
            reporters.set_active(0)

        hbox.pack_start(reporters,True)
        hbox.pack_start(action_list,True)
        hbox.pack_start(edit_actions,False)
        hbox.pack_start(remove_button,False)
        self.builder.get_object("vbActions").pack_start(hbox,False)
        hbox.show_all()

    def on_bAddAction_clicked(self, button):
        self.add_AnalyzerAction()

    def dehydrate(self):
        self.ccdaemon.setSettings(self.settings)

    def show(self):
        self.window.show()
