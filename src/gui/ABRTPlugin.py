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
from abrt_utils import _, log, log1, log2
from ConfBackend import getCurrentConfBackend, ConfBackendInitError

class PluginSettings(dict):
    def __init__(self):
        dict.__init__(self)
        self.client_side_conf = None
        try:
            self.client_side_conf = getCurrentConfBackend()
        except ConfBackendInitError, e:
            print e

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
            # FIXME: this fails when gk-authoriaztion fails
            # we need to show a dialog to user and let him know
            # for now just silently ignore it to avoid rhbz#559342
            settings = {}
            try:
                settings = self.client_side_conf.load(name)
            except Exception, e:
                print e
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
    types = {"":_("Not loaded plugins"),
             "Analyzer":_("Analyzer plugins"),
             "Action":_("Action plugins"),
             "Reporter":_("Reporter plugins"),
             "Database":_("Database plugins")}
    keys = ["WWW", "Name", "Enabled",
            "GTKBuilder", "Version",
            "Type", "Email", "Description"]

    def __init__(self):
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
            log("Plugin name is not set, can't load its settings")

    def save_settings_on_client_side(self):
        self.Settings.save_on_client_side(str(self.Name))
