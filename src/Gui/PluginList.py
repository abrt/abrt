# -*- coding: utf-8 -*-
import CCDBusBackend
from ABRTPlugin import PluginInfo, PluginSettings

class PluginInfoList(list):
    """Class to store list of PluginInfos"""
    def __init__(self,dbus_manager=None):
        self.dm = dbus_manager
        self.ddict = {}

    def load(self):
        if self.dm:
            #print "loading PluginList"
            try:
                rows = self.dm.getPluginsInfo()
                #print rows
                for row in rows:
                    entry = PluginInfo()
                    for column in row:
                        #print "PluginInfoList adding %s:%s" % (column,row[column])
                        entry.__dict__[column] = row[column]
                    if entry.Enabled == "yes":
                        #print ">>%s<<" % entry
                        entry.Settings = PluginSettings(self.dm.getPluginSettings(str(entry)))
                        #for i in entry.Settings.keys():
                        #    print "%s: %s" % (i, entry.Settings[i])
                    #else:
                    #    print "%s is disabled" % entry
                    self.append(entry)
                    self.ddict[entry.getName()] = entry
            except Exception, e:
                print e
                return
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
