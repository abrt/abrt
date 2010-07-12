import sys
import gtk
from PluginList import getPluginInfoList, PluginInfoList
from CC_gui_functions import *
from PluginSettingsUI import PluginSettingsUI
from ABRTPlugin import PluginSettings, PluginInfo
from abrt_utils import _, log, log1, log2


class PluginsSettingsDialog:
    def __init__(self, parent, daemon):
        #print "Settings dialog init"
        self.ccdaemon = daemon

        self.builder = gtk.Builder()
        builderfile = "%s%ssettings.glade" % (sys.path[0], "/")
        #print builderfile
        try:
            self.builder.add_from_file(builderfile)
        except Exception, e:
            print e
        self.window = self.builder.get_object("wPluginsSettings")
        if not self.window:
            raise Exception(_("Cannot load the GUI description for SettingsDialog!"))

        if parent:
            self.window.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
            self.window.set_transient_for(parent)
            self.window.set_modal(True)

        self.pluginlist = self.builder.get_object("tvSettings") # a TreeView
        # cell_text, toggle_active, toggle_visible, group_name_visible, color, plugin
        self.pluginsListStore = gtk.TreeStore(str, bool, bool, bool, str, object)
        # set filter
        modelfilter = self.pluginsListStore.filter_new()
        modelfilter.set_visible_func(self.filter_plugins, None)
        self.pluginlist.set_model(modelfilter)

        # Create/configure columns and add them to pluginlist
        # column "name" has two kind of cells:
        column = gtk.TreeViewColumn(_("Name"))
        # cells for individual plugins (white)
        cell_name = gtk.CellRendererText()
        column.pack_start(cell_name, True)
        column.set_attributes(cell_name, markup=0, visible=2) # show 0th field (plugin name) from data items if 2th field is true
        # cells for plugin types (gray)
        cell_plugin_type = gtk.CellRendererText()
        column.pack_start(cell_plugin_type, True)
        column.add_attribute(cell_plugin_type, "visible", 3)
        column.add_attribute(cell_plugin_type, "markup", 0)
        column.add_attribute(cell_plugin_type, "cell_background", 4)
        # column "name" is ready, insert
        column.set_resizable(True)
        self.pluginlist.append_column(column)

# "Enable" toggle column is disabled for now. Grep for PLUGIN_DYNAMIC_LOAD_UNLOAD
#        column = gtk.TreeViewColumn(_("Enabled"))
#        # column "enabled" has one kind of cells:
#        cell_toggle_enable = gtk.CellRendererToggle()
#        cell_toggle_enable.set_property("activatable", True)
#        cell_toggle_enable.connect("toggled", self.on_enabled_toggled, self.pluginsListStore)
#        column.pack_start(cell_toggle_enable, True)
#        column.add_attribute(cell_toggle_enable, "active", 1)
#        column.add_attribute(cell_toggle_enable, "visible", 2)
#        self.pluginlist.append_column(column)

        #connect signals
        self.pluginlist.connect("cursor-changed", self.on_tvDumps_cursor_changed)
        self.builder.get_object("bConfigurePlugin").connect("clicked", self.on_bConfigurePlugin_clicked, self.pluginlist)
        self.builder.get_object("bClose").connect("clicked", self.on_bClose_clicked)
        self.builder.get_object("bConfigurePlugin").set_sensitive(False)

# "Enable" toggle column is disabled for now. Grep for PLUGIN_DYNAMIC_LOAD_UNLOAD
#    def on_enabled_toggled(self,cell, path, model):
#        plugin = model[path][model.get_n_columns()-1]
#        if plugin:
#            if model[path][1]:
#                #print "self.ccdaemon.UnRegisterPlugin(%s)" % (plugin.getName())
#                self.ccdaemon.unRegisterPlugin(plugin.getName())
#                # FIXME: create class plugin and move this into method Plugin.Enable()
#                plugin.Enabled = "no"
#                plugin.Settings = None
#            else:
#                #print "self.ccdaemon.RegisterPlugin(%s)" % (model[path][model.get_n_columns()-1])
#                self.ccdaemon.registerPlugin(plugin.getName())
#                # FIXME: create class plugin and move this into method Plugin.Enable()
#                plugin.Enabled = "yes"
#                default_settings = self.ccdaemon.getPluginSettings(plugin.getName())
#                plugin.Settings = PluginSettings()
#                plugin.Settings.load(plugin.getName(), default_settings)
#            model[path][1] = not model[path][1]

    def filter_plugins(self, model, miter, data):
        return True

    def hydrate(self):
        #print "settings hydrate"
        self.pluginsListStore.clear()
        try:
            #pluginlist = getPluginInfoList(self.ccdaemon, refresh=True)
            # don't force refresh as it will overwrite settings if g-k is not available
            pluginlist = getPluginInfoList(self.ccdaemon)
        except Exception, e:
            log("Error while loading plugins info: %s", e)
            #gui_error_message("Error while loading plugins info, please check if abrt daemon is running\n %s" % e)
            return
        plugin_rows = {}
        group_empty = {}
        for plugin_type in PluginInfo.types.keys():
            it = self.pluginsListStore.append(None,
                        # cell_text, toggle_active, toggle_visible, group_name_visible, color, plugin
                        ["<b>%s</b>" % PluginInfo.types[plugin_type], 0, 0, 1, "gray", None])
            plugin_rows[plugin_type] = it
            group_empty[plugin_type] = it
        for entry in pluginlist:
            if entry.Description:
                text = "<b>%s</b>\n%s" % (entry.getName(), entry.Description)
            else:
                # non-loaded plugins have empty description
                text = "<b>%s</b>" % entry.getName()
            plugin_type = entry.getType()
            self.pluginsListStore.append(plugin_rows[plugin_type],
                        # cell_text, toggle_active, toggle_visible, group_name_visible, color, plugin
                        [text, entry.Enabled == "yes", 1, 0, "white", entry])
            if group_empty.has_key(plugin_type):
                del group_empty[plugin_type]
        # rhbz#560971 "Don't show empty 'Not loaded plugins' section"
        # don't show any empty groups
        for it in group_empty.values():
            self.pluginsListStore.remove(it)

        self.pluginlist.expand_all()

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
            gui_info_dialog(_("Please select a plugin from the list to edit it's options."), parent=self.window)
            return
        # this should work until we keep the row object in the last position
        pluginfo = pluginsListStore.get_value(pluginsListStore.get_iter(path[0]), pluginsListStore.get_n_columns()-1)
        if pluginfo:
            try:
                ui = PluginSettingsUI(pluginfo, parent=self.window)
            except Exception, e:
                gui_error_message(_("Error while opening the plugin settings UI: \n\n%s" % e))
                return
            ui.hydrate()
            response = ui.run()
            if response == gtk.RESPONSE_APPLY:
                ui.dehydrate()
                if pluginfo.Settings:
                    try:
                        pluginfo.save_settings_on_client_side()
                        # FIXME: do we need to call this? all reporters set their settings
                        # when Report() is called
                        self.ccdaemon.setPluginSettings(pluginfo.getName(), pluginfo.Settings)
                    except Exception, e:
                        gui_error_message(_("Cannot save plugin settings:\n %s" % e))
                #for key, val in pluginfo.Settings.iteritems():
                #    print "%s:%s" % (key, val)
            elif response == gtk.RESPONSE_CANCEL:
                pass
            else:
                log("unknown response from settings dialog:%d", response)
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
        if pluginfo:
            self.builder.get_object("lPluginAuthor").set_text(pluginfo.Email)
            self.builder.get_object("lPluginVersion").set_text(pluginfo.Version)
            self.builder.get_object("lPluginWebSite").set_text(pluginfo.WWW)
            self.builder.get_object("lPluginName").set_text(pluginfo.Name)
            self.builder.get_object("lPluginDescription").set_text(pluginfo.Description)
    #        print (pluginfo.Enabled == "yes" and pluginfo.GTKBuilder != "")
        self.builder.get_object("bConfigurePlugin").set_sensitive(pluginfo != None and pluginfo.Enabled == "yes" and pluginfo.GTKBuilder != "")
