import CCDBusBackend
from CCDump import Dump

class DumpList(list):
    """Class to store list of debug dumps"""
    def __init__(self,dbus_manager=None):
        self.dm = dbus_manager
    
    def load(self):
        if self.dm:
            print "loading DumpList"
            try:
                rows = self.dm.getDumps()
                #print rows
                for row in rows:
                    entry = Dump()
                    for column in row:
                        #print "DumpList adding %s:%s" % (column,row[column])
                        entry.__dict__[column] = row[column]
                    self.append(entry)
            except Exception, e:
                print e
                return
        else:
            print "db == None!"
                

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
