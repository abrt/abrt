import sys
import gtk
from PluginList import getPluginInfoList, PluginInfoList
from CC_gui_functions import *
from PluginSettingsUI import PluginSettingsUI
from ABRTPlugin import PluginSettings

class SettingsDialog:
    def __init__(self, parent, daemon):
        #print "Settings dialog init"
        self.ccdaemon = daemon
        self.builder = gtk.Builder()
        builderfile = "%s%ssettings.GtkBuilder" % (sys.path[0],"/")
        #print builderfile
        try:
            self.builder.add_from_file(builderfile)
        except Exception, e:
            print e
        self.window = self.builder.get_object("wSettings")
        if not self.window:
            raise Exception("Can't load gui description for SettingsDialog!")
        #self.window.set_parent(parent)

        self.pluginlist = self.builder.get_object("tvSettings")
        self.pluginsListStore = gtk.ListStore(str, bool, object)
        # set filter
        self.modelfilter = self.pluginsListStore.filter_new()
        self.modelfilter.set_visible_func(self.filter_plugins, None)
        self.pluginlist.set_model(self.modelfilter)
        # ===============================================
        columns = [None]*1
        columns[0] = gtk.TreeViewColumn('Name')
        
        # create list
        for column in columns:
            n = self.pluginlist.append_column(column)
            column.cell = gtk.CellRendererText()
            column.pack_start(column.cell, False)
            column.set_attributes(column.cell, markup=(n-1))
            column.set_resizable(True)
            
        # toggle
        toggle_renderer = gtk.CellRendererToggle()
        toggle_renderer.set_property('activatable', True)
        toggle_renderer.connect( 'toggled', self.on_enabled_toggled, self.pluginsListStore )
        column = gtk.TreeViewColumn('Enabled', toggle_renderer)
        column.add_attribute( toggle_renderer, "active", 1)
        self.pluginlist.insert_column(column, 0)
            
        #connect signals
        self.pluginlist.connect("cursor-changed", self.on_tvDumps_cursor_changed)
        self.builder.get_object("bConfigurePlugin").connect("clicked", self.on_bConfigurePlugin_clicked, self.pluginlist)
        self.builder.get_object("bClose").connect("clicked", self.on_bClose_clicked)

    def on_enabled_toggled(self,cell, path, model):
        plugin = model[path][model.get_n_columns()-1]
        if model[path][1]:
            #print "self.ccdaemon.UnRegisterPlugin(%s)" % (plugin.getName())
            self.ccdaemon.unRegisterPlugin(plugin.getName())
            # FIXME: create class plugin and move this into method Plugin.Enable()
            plugin.Enabled = "no"
            plugin.Settings = None
        else:
            #print "self.ccdaemon.RegisterPlugin(%s)" % (model[path][model.get_n_columns()-1])
            self.ccdaemon.registerPlugin(plugin.getName())
            # FIXME: create class plugin and move this into method Plugin.Enable()
            plugin.Enabled = "yes"
            plugin.Settings = PluginSettings(self.ccdaemon.getPluginSettings(plugin.getName()))
        model[path][1] = not model[path][1]    

    def filter_plugins(self, model, miter, data):
        return True
    def hydrate(self):
        #print "settings hydrate"
        self.pluginsListStore.clear()
        try:
            pluginlist = getPluginInfoList(self.ccdaemon, refresh=True)
        except Exception, e:
            print e
            #gui_error_message("Error while loading plugins info, please check if abrt daemon is running\n %s" % e)
        for entry in pluginlist:
                n = self.pluginsListStore.append(["<b>%s</b>\n%s" % (entry.getName(), entry.Description), entry.Enabled == "yes", entry])

    def dehydrate(self):
        # we have nothing to save, plugin's does the work
        pass

    def show(self):
        self.window.show()
        #if result == gtk.RESPONSE_APPLY:
        #    self.dehydrate()
        #self.window.destroy()
        #return result

    def on_bConfigurePlugin_clicked(self, button, pluginview):
        pluginsListStore, path = pluginview.get_selection().get_selected_rows()
        if not path:
            self.builder.get_object("lDescription").set_label("ARGH...")
            return
        # this should work until we keep the row object in the last position
        pluginfo = pluginsListStore.get_value(pluginsListStore.get_iter(path[0]), pluginsListStore.get_n_columns()-1)
        try:
            ui = PluginSettingsUI(pluginfo)
        except Exception, e:
            gui_error_message("Error while opening plugin settings UI: \n\n%s" % e)
            return
        ui.hydrate()
        response = ui.run()
        if response == gtk.RESPONSE_APPLY:
            ui.dehydrate()
            if pluginfo.Settings:
                try:
                    self.ccdaemon.setPluginSettings(pluginfo.getName(), pluginfo.Settings)
                except Exception, e:
                    gui_error_message("Can't save plugin settings:\n %s", e)
            #for key, val in pluginfo.Settings.iteritems():
            #    print "%s:%s" % (key, val)
        elif response == gtk.RESPONSE_CANCEL:
            pass
        else:
            print "unknown response from settings dialog"
        ui.destroy()
    
    def on_bClose_clicked(self, button):
        self.window.destroy()

    def on_tvDumps_cursor_changed(self, treeview):
        pluginsListStore, path = treeview.get_selection().get_selected_rows()
        if not path:
            self.builder.get_object("lDescription").set_label("No description")
            return
        # this should work until we keep the row object in the last position
        pluginfo = pluginsListStore.get_value(pluginsListStore.get_iter(path[0]), pluginsListStore.get_n_columns()-1)
        self.builder.get_object("lPluginAuthor").set_text(pluginfo.Email)
        self.builder.get_object("lPluginVersion").set_text(pluginfo.Version)
        self.builder.get_object("lPluginWebSite").set_text(pluginfo.WWW)
        self.builder.get_object("lPluginName").set_text(pluginfo.Name)
        self.builder.get_object("lPluginDescription").set_text(pluginfo.Description)
#        print (pluginfo.Enabled == "yes" and pluginfo.GTKBuilder != "")
        self.builder.get_object("bConfigurePlugin").set_sensitive(pluginfo.Enabled == "yes" and pluginfo.GTKBuilder != "")
