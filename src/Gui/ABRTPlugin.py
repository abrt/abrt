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
from ConfBackend import ConfBackendGnomeKeyring, ConfBackendInitError

class PluginSettings(dict):
    def __init__(self):
        dict.__init__(self)
        self.client_side_conf = None
        try:
            self.client_side_conf = ConfBackendGnomeKeyring()
        except ConfBackendInitError, e:
            print e
            pass

    def check(self):
        # if present, these should be non-empty
        for key in ["Password", "Login"]:
            if key in self.keys():
                if not self[key]:
                    # some of the required keys are missing
                    return False
        # settings are OK
        return True

    def load_daemon_settings(self, name, daemon_settings):
        # load settings from daemon
        for key in daemon_settings.keys():
            self[str(key)] = str(daemon_settings[key])

        if self.client_side_conf:
            settings = self.client_side_conf.load(name)
            # overwrite daemon data with user setting
            for key in settings.keys():
                # only rewrite keys which exist in plugin's keys.
                # e.g. we don't want a password field for logger plugin
                if key in daemon_settings.keys():
                    self[str(key)] = str(settings[key])

    def save_on_client_side(self, name):
        if self.client_side_conf:
            self.client_side_conf.save(name, self)

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

    def load_daemon_settings(self, daemon_settings):
        if self.Name:
            self.Settings.load_daemon_settings(self.Name, daemon_settings)
        else:
            print _("Plugin name is not set, can't load its settings")

    def save_settings_on_client_side(self):
        self.Settings.save_on_client_side(str(self.Name))
