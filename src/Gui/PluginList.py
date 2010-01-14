# -*- coding: utf-8 -*-
import CCDBusBackend
from ABRTPlugin import PluginInfo, PluginSettings

class PluginInfoList(list):
    """Class to store list of PluginInfos"""
    def __init__(self,dbus_manager=None):
        self.dm = dbus_manager

    def load(self):
        if self.dm:
            #print "loading PluginList"
            rows = self.dm.getPluginsInfo()
            #print rows
            for plugin_name in rows:
                row = rows[plugin_name]
                entry = PluginInfo()
                for attr_name in row:
                    #print "PluginInfoList adding %s[%s]:%s" % (plugin_name, attr_name, row[attr_name])
                    entry.__dict__[attr_name] = row[attr_name]
                daemon_settings = self.dm.getPluginSettings(plugin_name)
                entry.load_daemon_settings(daemon_settings)
                self.append(entry)
        else:
            print "db == None!"

    def getEnabledPlugins(self):
        return [x for x in self if x["Enabled"] == 'yes']

    def getActionPlugins(self):
        return [x for x in self if x["Enabled"] == 'yes' and x["Type"] == 'Action']

    def getDatabasePlugins(self):
        return [x for x in self if x["Enabled"] == 'yes' and x["Type"] == 'Database']

    def getAnalyzerPlugins(self):
        return [x for x in self if x["Enabled"] == 'yes' and x["Type"] == 'Analyzer']

    def getReporterPlugins(self):
        return [x for x in self if x["Enabled"] == 'yes' and x["Type"] == 'Reporter']



__PFList = None
__PFList_dbmanager = None

def getPluginInfoList(dbmanager,refresh=None):
    global __PFList
    global __PFList_dbmanager

    if __PFList == None or refresh or __PFList_dbmanager != dbmanager:
        __PFList = PluginInfoList(dbus_manager=dbmanager)
        __PFList.load()
        __PFList_dbmanager = dbmanager
    return __PFList

__PFList = None
