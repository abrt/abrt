import sys
import gtk
from PluginList import getPluginInfoList, PluginInfoList
from CC_gui_functions import *
#from PluginSettingsUI import PluginSettingsUI
from ABRTPlugin import PluginSettings, PluginInfo
from abrt_utils import _

class SettingsDialog:
    def __init__(self, parent, daemon):
        builderfile = "%s%ssettings.GtkBuilder" % (sys.path[0],"/")
        self.ccdaemon = daemon
        self.builder = gtk.Builder()
        self.builder.add_from_file(builderfile)
        self.window = self.builder.get_object("wGlobalSettings")
        print "GSD init"
        self.builder.get_object("bSaveSettings").connect("clicked", self.on_ok_clicked)
        self.builder.get_object("bAddCronJob").connect("clicked", self.on_bAddCronJob_clicked)
        
    def filter_settings(self, model, miter, data):
        return True
        
    def hydrate(self):
        try:
            self.settings = self.ccdaemon.getSettings()
        except Exception, e:
            # FIXME: this should be error gui message!
            print e
        
        # hydrate cron jobs:
        for key,val in self.settings["Cron"].iteritems():
            try:
                self.pluginlist = getPluginInfoList(self.ccdaemon, refresh=True)
            except Exception, e:
                print e
                
            hbox = gtk.HBox(homogeneous=True)
            time = gtk.Entry()
            plugins = gtk.ComboBox()    
            enabledPluginsListStore = gtk.ListStore(str, object)
            cell = gtk.CellRendererText()
            plugins.pack_start(cell)
            plugins.add_attribute(cell, 'text', 0)
            enabledPluginsListStore.append([_("Select a plugin"), None])
            for plugin in self.pluginlist.getActionPlugins():
                print "#", plugin.getName()
                enabledPluginsListStore.append([plugin.getName(), plugin])
            plugins.set_model(enabledPluginsListStore)
            plugins.set_active(0)
            hbox.pack_start(time,False)
            hbox.pack_start(plugins,False)
            self.builder.get_object("vbCronJobs").pack_start(hbox,False)
            hbox.show_all()
            #print "\t%s:%s" % (key,val)
        
    def on_ok_clicked(self, button):
        self.dehydrate()
        
    def on_bAddCronJob_clicked(self, button):
        hbox = gtk.HBox(homogeneous=True)
        time = gtk.Entry()
        plugins = gtk.ComboBox()    
        enabledPluginsListStore = gtk.ListStore(str, object)
        cell = gtk.CellRendererText()
        plugins.pack_start(cell)
        plugins.add_attribute(cell, 'text', 0)
        for plugin in self.pluginlist.getActionPlugins():
            print "#", plugin.getName()
            enabledPluginsListStore.append([plugin.getName(), plugin])
        plugins.set_model(enabledPluginsListStore)
        plugins.set_active(0)
        hbox.pack_start(time,False)
        hbox.pack_start(plugins,False)
        self.builder.get_object("vbCronJobs").pack_start(hbox,False)
        hbox.show_all()
        print "add"
        
    def on_cancel_clicked(self,button):
        print "hide"
        self.window.hide()
        
    def dehydrate(self):
        self.ccdaemon.setSettings(self.settings)
        print "dehydrate"
        
    def show(self):
        print "show"
        self.window.show()
