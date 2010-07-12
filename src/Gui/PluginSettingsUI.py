import gtk
from abrt_utils import _, log, log1, log2

class PluginSettingsUI():
    def __init__(self, pluginfo, parent=None):
        #print "Init PluginSettingsUI"
        self.plugin_name = pluginfo.Name
        self.Settings = pluginfo.Settings
        self.pluginfo = pluginfo
        self.plugin_gui = None

        if pluginfo.getGUI():
            self.plugin_gui = gtk.Builder()
            self.plugin_gui.add_from_file(pluginfo.getGUI())
            self.dialog = self.plugin_gui.get_object("PluginDialog")
            if not self.dialog:
                raise Exception(_("Cannot find PluginDialog widget in the UI description!"))
            self.dialog.set_title("%s" % pluginfo.getName())
            if parent:
                self.dialog.set_transient_for(parent)
        else:
            # we shouldn't get here, but just to be safe
            log1("No UI for plugin %s" % pluginfo)
            raise Exception(_("No UI for the plugin <b>%s</b>, this is probably a bug.\n"
                              "Please report it at "
                              "<a href=\"https://fedorahosted.org/abrt/newticket\">"
                              "https://fedorahosted.org/abrt/newticket</a>") % pluginfo)
            return

        if parent:
            self.dialog.set_position(gtk.WIN_POS_CENTER_ON_PARENT)
            self.dialog.set_transient_for(parent)
            self.dialog.set_modal(True)

    def on_show_pass_toggled(self, button, entry=None):
        if entry:
            entry.set_visibility(button.get_active())

    def hydrate(self):
        if self.plugin_gui:
            if self.pluginfo.Enabled == "yes":
                if self.Settings:
                    #print "Hydrating %s" % self.plugin_name
                    for key, value in self.Settings.iteritems():
                        #print "%s:%s" % (key,value)
                        widget = self.plugin_gui.get_object("conf_%s" % key)
                        if type(widget) == gtk.Entry:
                            widget.set_text(value)
                            if widget.get_visibility() == False:
                                # if we find toggle button called the same name as entry and entry has
                                # visibility set to False, connect set_visible to it
                                # coz I guess it's toggle for revealing the password
                                button = self.plugin_gui.get_object("cb_%s" % key)
                                if type(button) == gtk.CheckButton:
                                    button.connect("toggled", self.on_show_pass_toggled, widget)
                        elif type(widget) == gtk.CheckButton:
                            widget.set_active(value == "yes")
                        elif type(widget) == gtk.ComboBox:
                            print _("Combo box is not implemented")
                else:
                    #print "Plugin %s has no configuration." % self.plugin_name
                    pass
            else:
                #print "Plugin %s is disabled." % self.plugin_name
                pass

        else:
            print _("Nothing to hydrate!")

    def dehydrate(self):
        #print "dehydrating %s" % self.pluginfo.getName()
        if self.Settings:
            for key in self.Settings.keys():
                #print key
                #print "%s:%s" % (key,value)
                widget = self.plugin_gui.get_object("conf_%s" % key)
                if type(widget) == gtk.Entry:
                    self.Settings[key] = widget.get_text()
                elif type(widget) == gtk.CheckButton:
                    if widget.get_active():
                        self.Settings[key] = "yes"
                    else:
                        self.Settings[key] = "no"
                elif type(widget) == gtk.ComboBox:
                    print _("Combo box is not implemented")

    def destroy(self):
        self.dialog.destroy()

    def run(self):
        return self.dialog.run()
