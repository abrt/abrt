import gtk

class PluginSettingsUI(gtk.Dialog):
    def __init__(self, pluginfo):
        #print "Init PluginSettingsUI"
        gtk.Dialog.__init__(self)
        self.plugin_name = pluginfo.Name
        self.Settings = pluginfo.Settings
        self.pluginfo = pluginfo
        self.plugin_gui = None
        
        if pluginfo.getGUI():
            self.plugin_gui = gtk.Builder()
            self.plugin_gui.add_from_file(pluginfo.getGUI())
            self.dialog = self.plugin_gui.get_object("PluginDialog")
            if not self.dialog:
                raise Exception("Can't find PluginDialog widget in UI description!")
            self.dialog.set_title("%s" % pluginfo.getName())
        else:
            # we shouldn't get here, but just to be safe
            no_ui_label = gtk.Label("No UI for plugin %s" % pluginfo)
            self.add(no_ui_label)
            no_ui_label.show()
        
    def hydrate(self):
        if self.plugin_gui:
            if self.pluginfo.Enabled == "yes":
                if self.Settings:
                    #print "Hydrating %s" % self.plugin_name
                    for key,value in self.Settings.iteritems():
                        #print "%s:%s" % (key,value)
                        widget = self.plugin_gui.get_object("conf_%s" % key)
                        if type(widget) == gtk.Entry:
                            widget.set_text(value)
                        elif type(widget) == gtk.CheckButton:
                            widget.set_active(value == "yes")
                        elif type(widget) == gtk.ComboBox:
                            print "combo box is not implemented"
                else:
                    #print "Plugin %s has no configuration." % self.plugin_name
                    pass
            else:
                #print "Plugin %s is disabled." % self.plugin_name
                pass
            
        else:
            print "Nothing to hydrate!"
            
    def dehydrate(self):
        #print "dehydrating %s" % self.pluginfo.getName()
        pass
    
    def destroy(self):
        self.dialog.destroy()
    
    def run(self):
        return self.dialog.run()
