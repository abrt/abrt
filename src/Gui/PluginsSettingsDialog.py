import sys
import gtk
from PluginList import getPluginInfoList, PluginInfoList
from CC_gui_functions import *
from PluginSettingsUI import PluginSettingsUI
from ABRTPlugin import PluginSettings, PluginInfo
from abrt_utils import _

class PluginsSettingsDialog:
    def __init__(self, parent, daemon):
        #print "Settings dialog init"
        self.ccdaemon = daemon
        self.builder = gtk.Builder()
        builderfile = "%s%ssettings.glade" % (sys.path[0],"/")
        #print builderfile
        try:
            self.builder.add_from_file(builderfile)
        except Exception, e:
            print e
        self.window = self.builder.get_object("wPluginsSettings")
        if not self.window:
            raise Exception(_("Can't load gui description for SettingsDialog!"))
        #self.window.set_parent(parent)

        self.pluginlist = self.builder.get_object("tvSettings")
        # cell_text, toggle_active, toggle_visible, group_name_visible, color, plugin
        self.pluginsListStore = gtk.TreeStore(str, bool, bool, bool, str, object)
        # set filter
        self.modelfilter = self.pluginsListStore.filter_new()
        self.modelfilter.set_visible_func(self.filter_plugins, None)
        self.pluginlist.set_model(self.modelfilter)
        # ===============================================
        columns = [None]*1
        columns[0] = gtk.TreeViewColumn(_("Name"))

        # create list
        for column in columns:
            n = self.pluginlist.append_column(column)
            column.cell = gtk.CellRendererText()
            column.gray_background = gtk.CellRendererText()
            column.pack_start(column.cell, True)
            column.pack_start(column.gray_background, True)
            column.set_attributes(column.cell, markup=(n-1), visible=2)
            column.set_attributes(column.gray_background, visible=3, cell_background=4)
            column.set_resizable(True)

        # toggle
        group_name_renderer = gtk.CellRendererText()
        toggle_renderer = gtk.CellRendererToggle()
        toggle_renderer.set_property('activatable', True)
        toggle_renderer.connect( 'toggled', self.on_enabled_toggled, self.pluginsListStore )
        column = gtk.TreeViewColumn(_('Enabled'))
        column.pack_start(toggle_renderer, True)
        column.pack_start(group_name_renderer, True)
        column.add_attribute( toggle_renderer, "active", 1)
        column.add_attribute( toggle_renderer, "visible", 2)
        column.add_attribute( group_name_renderer, "visible", 3)
        column.add_attribute( group_name_renderer, "markup", 0)
        column.add_attribute( group_name_renderer, "cell_background", 4)
        self.pluginlist.insert_column(column, 0)

        #connect signals
        self.pluginlist.connect("cursor-changed", self.on_tvDumps_cursor_changed)
        self.builder.get_object("bConfigurePlugin").connect("clicked", self.on_bConfigurePlugin_clicked, self.pluginlist)
        self.builder.get_object("bClose").connect("clicked", self.on_bClose_clicked)
        self.builder.get_object("bConfigurePlugin").set_sensitive(False)

    def on_enabled_toggled(self,cell, path, model):
        plugin = model[path][model.get_n_columns()-1]
        if plugin:
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
                default_settings = self.ccdaemon.getPluginSettings(plugin.getName())
                plugin.Settings = PluginSettings()
                plugin.Settings.load(plugin.getName(), default_settings)
            model[path][1] = not model[path][1]

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
            print e
            #gui_error_message("Error while loading plugins info, please check if abrt daemon is running\n %s" % e)
        plugin_rows = {}
        for plugin_type in PluginInfo.types.keys():
            it = self.pluginsListStore.append(None, ["<b>%s</b>" % (PluginInfo.types[plugin_type]),0 , 0, 1,"gray", None])
            plugin_rows[plugin_type] = it
        for entry in pluginlist:
            n = self.pluginsListStore.append(plugin_rows[entry.getType()],["<b>%s</b>\n%s" % (entry.getName(), entry.Description), entry.Enabled == "yes", 1, 0, "white", entry])
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
            self.builder.get_object("lDescription").set_label(_("Can't get plugin description"))
            return
        # this should work until we keep the row object in the last position
        pluginfo = pluginsListStore.get_value(pluginsListStore.get_iter(path[0]), pluginsListStore.get_n_columns()-1)
        if pluginfo:
            try:
                ui = PluginSettingsUI(pluginfo)
            except Exception, e:
                gui_error_message(_("Error while opening plugin settings UI: \n\n%s" % e))
                return
            ui.hydrate()
            response = ui.run()
            if response == gtk.RESPONSE_APPLY:
                ui.dehydrate()
                if pluginfo.Settings:
                    try:
                        pluginfo.save_settings()
                        # FIXME: do we need to call this? all reporters set their settings
                        # when Report() is called
                        self.ccdaemon.setPluginSettings(pluginfo.getName(), pluginfo.Settings)
                    except Exception, e:
                        gui_error_message(_("Can't save plugin settings:\n %s" % e))
                #for key, val in pluginfo.Settings.iteritems():
                #    print "%s:%s" % (key, val)
            elif response == gtk.RESPONSE_CANCEL:
                pass
            else:
                print _("unknown response from settings dialog")
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
