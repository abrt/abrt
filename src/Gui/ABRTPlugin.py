# -*- coding: utf-8 -*-

""" PluginInfo keys:
WWW
Name
Enabled
GTKBuilder
Version
Type
Email
Description
"""
from abrt_utils import _
from ConfBackend import ConfBackendGnomeKeyring

class PluginSettings(dict):
    def __init__(self):
        dict.__init__(self)
        #print "Init plugin settings"

    def __init__(self, settings_dict):
        dict.__init__(self)
        for key in settings_dict.keys():
            self[key] = settings_dict[key]
            
    def check(self):
        if "Password" in self.keys():
            # password is missing
            if not self["Password"]:
                return False
        # settings are OK
        return True
    
    def load(self, name):
        print "load:", name
    
    def save(self, name):
        print "save: ", name
        
class PluginInfo():
    """Class to represent common plugin info"""
    types = {"Analyzer":_("Analyzer plugins"),
             "Action":_("Action plugins"),
             "Reporter":_("Reporter plugins"),
             "Database":_("Database plugins")}
    keys = ["WWW", "Name", "Enabled",
            "GTKBuilder", "Version",
            "Type", "Email", "Description"]

    def __init__(self):
        #print "Init PluginInfo"
        self.WWW = None
        self.Name = None
        self.Enabled = None
        self.GTKBuilder = None
        self.Version = None
        self.Type = None
        self.Email = None
        self.Description = None
        self.Settings = PluginSettings({})

    def getName(self):
        return self.Name

    def getDescription(self):
        return self.Description

    def getType(self):
        return self.Type

    def getGUI(self):
        return self.GTKBuilder

    def __str__(self):
        return self.Name

    def __getitem__(self, item):
        return self.__dict__[item]
    
    def load_settings(self):
        if self.Name:
            self.Settings.load(self.Name)
        else:
            print "plugin name is not set, can't load it's settings"
    
    def save_settings(self):
        self.Settings.save(self.Name)
