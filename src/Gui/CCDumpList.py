# -*- coding: utf-8 -*-
from CCDump import Dump

from abrt_utils import _, init_logging, log, log1, log2

class DumpList(list):
    """Class to store list of debug dumps"""
    def __init__(self,dbus_manager=None):
        list.__init__(self)
        self.dm = dbus_manager

    def load(self):
        if self.dm:
            #print "loading DumpList"
            try:
                rows = self.dm.getDumps()
                #print rows
                for row in rows:
                    entry = Dump()
                    for column in row:
                        log2(" Dump.%s='%s'", column, row[column])
                        entry.__setattr__(column, row[column])
                    self.append(entry)
            except Exception:
                # FIXME handle exception better
                # this is just temporary workaround for rhbz#543725
                raise
        else:
            print "db == None!"

    def getDumpByCrashID(self, crashid):
        for dump in self:
            # crashid can be either hash or uid:hash
            if crashid in (dump.getUUID(),dump.getUID()+":"+dump.getUUID()):
                return dump

__PFList = None
__PFList_dbmanager = None

def getDumpList(dbmanager,refresh=None):
    global __PFList
    global __PFList_dbmanager

    if __PFList == None or refresh or __PFList_dbmanager != dbmanager:
        __PFList = DumpList(dbus_manager=dbmanager)
        __PFList.load()
        __PFList_dbmanager = dbmanager
    return __PFList

__PFList = None
