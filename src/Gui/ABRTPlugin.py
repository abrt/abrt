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
        self.conf = ConfBackendGnomeKeyring()

    def check(self):
        for key in ["Password", "Login"]:
            if key in self.keys():
                # some of the required keys is missing
                if not self[key]:
                    return False
        # settings are OK
        return True

    def load(self, name, default_settings):
        # load settings from daemon
        for key in default_settings.keys():
            self[str(key)] = str(default_settings[key])

        settings = self.conf.load(name)
        # overwrite defaluts with user setting
        for key in settings.keys():
            self[str(key)] = str(settings[key])

    def save(self, name):
        self.conf.save(name, self)

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
        self.Settings = PluginSettings()

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

    def load_settings(self, default_settings):
        if self.Name:
            self.Settings.load(self.Name, default_settings)
        else:
            print _("Plugin name is not set, can't load it's settings")

    def save_settings(self):
        self.Settings.save(str(self.Name))
