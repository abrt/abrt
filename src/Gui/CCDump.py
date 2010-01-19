# -*- coding: utf-8 -*-
from datetime import datetime

from abrt_utils import _, init_logging, log, log1, log2

CD_TYPE     = 0
CD_EDITABLE = 1
CD_CONTENT  = 2

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
        self.Message = None
        self.Reported = None

    def getUUID(self):
        return self.UUID[CD_CONTENT]

    def getUID(self):
        return self.UID[CD_CONTENT]

    def getCount(self):
        return self.Count[CD_CONTENT]

    def getExecutable(self):
        return self.Executable[CD_CONTENT]

    def getPackage(self):
        return self.Package[CD_CONTENT]

    def isReported(self):
        return self.Reported[CD_CONTENT] == "1"

    def getMessage(self):
        if not self.Message:
            return "" #[]
        #return self.Message[CD_CONTENT].split('\n')
        return self.Message[CD_CONTENT]

    def getTime(self,format):
        #print format
        if format:
            try:
                return datetime.fromtimestamp(int(self.Time[CD_CONTENT])).strftime(format)
            except Exception, e:
                print e
        return int(self.Time[CD_CONTENT])

    def getPackageName(self):
        return self.Package[CD_CONTENT][:self.Package[CD_CONTENT].find("-")]

    def getDescription(self):
        return self.Description[CD_CONTENT]
