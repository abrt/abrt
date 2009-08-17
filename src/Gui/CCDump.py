# -*- coding: utf-8 -*-
from datetime import datetime

TYPE = 0
EDITABLE = 1
CONTENT = 2

class Dump():
    """Class for mapping the debug dump to python object"""
    def __init__(self):
        self.UUID = None
        self.UID = None
        self.Count = None
        self.Executable = None
        self.Package = None
        self.Time = None
        self.Description = None
        self.Reported = None
    
    def getUUID(self):
        return self.UUID[CONTENT]
    
    def getUID(self):
        return self.UID[CONTENT]
        
    def getCount(self):
        return self.Count[CONTENT]
    
    def getExecutable(self):
        return self.Executable[CONTENT]
        
    def getPackage(self):
        return self.Package[CONTENT]
    
    def isReported(self):
        return self.Reported[CONTENT] == "1"
    
    def getTime(self,format):
        #print format
        if format:
            try:
                return datetime.fromtimestamp(int(self.Time[CONTENT])).strftime(format)
            except Exception, e:
                print e
        return int(self.Time[CONTENT])
        
    def getPackageName(self):
        return self.Package[CONTENT][:self.Package[CONTENT].find("-")]
        
    def getDescription(self):
        return self.Description[CONTENT]
