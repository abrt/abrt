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

class PluginSettings(dict):
    def __init__(self):
        #print "Init plugin settings"
        pass

    def __init__(self, settings_dict):
        for key in settings_dict.keys():
            self[key] = settings_dict[key]

"""Class to represent common plugin info"""
class PluginInfo():
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
        self.Settings = None

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

