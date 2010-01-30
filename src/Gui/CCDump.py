# -*- coding: utf-8 -*-
from datetime import datetime

from abrt_utils import _, init_logging, log, log1, log2

# Should match CrashTypes.h!
CD_TYPE     = 0
CD_EDITABLE = 1
CD_CONTENT  = 2

CD_SYS = "s"
CD_BIN = "b"
CD_TXT = "t"

FILENAME_ARCHITECTURE = "architecture"
FILENAME_KERNEL       = "kernel"
FILENAME_TIME         = "time"
FILENAME_UID          = "uid"
FILENAME_PACKAGE      = "package"
FILENAME_COMPONENT    = "component"
FILENAME_DESCRIPTION  = "description"
FILENAME_ANALYZER     = "analyzer"
FILENAME_RELEASE      = "release"
FILENAME_EXECUTABLE   = "executable"
FILENAME_REASON       = "reason"
FILENAME_COMMENT      = "comment"
FILENAME_REPRODUCE    = "reproduce"
FILENAME_RATING       = "rating"
FILENAME_CMDLINE      = "cmdline"
FILENAME_COREDUMP     = "coredump"
FILENAME_BACKTRACE    = "backtrace"
FILENAME_MEMORYMAP    = "memorymap"
FILENAME_KERNELOOPS   = "kerneloops"

CD_DUPHASH      = "DUPHASH"
CD_UUID         = "UUID"
CD_DUMPDIR      = "DumpDir"
CD_COUNT        = "Count"
CD_REPORTED     = "Reported"
CD_MESSAGE      = "Message"

# FIXME - create method or smth that returns type|editable|content


class Dump():
    """Class for mapping the debug dump to python object"""
    def __init__(self):
        self.UUID = None
        self.uid = None
        self.Count = None
        self.executable = None
        self.package = None
        self.time = None
        self.description = None
        self.Message = None
        self.Reported = None

    def getUUID(self):
        return self.UUID[CD_CONTENT]

    def getUID(self):
        return self.uid[CD_CONTENT]

    def getCount(self):
        return self.Count[CD_CONTENT]

    def getExecutable(self):
        return self.executable[CD_CONTENT]

    def getPackage(self):
        return self.package[CD_CONTENT]

    def isReported(self):
        return self.Reported[CD_CONTENT] == "1"

    def getMessage(self):
        if not self.Message:
            return "" #[]
        #return self.Message[CD_CONTENT].split('\n')
        return self.Message[CD_CONTENT]

    def getTime(self, fmt):
        #print format
        if fmt:
            try:
                return datetime.fromtimestamp(int(self.time[CD_CONTENT])).strftime(fmt)
            except Exception, e:
                print e
        return int(self.time[CD_CONTENT])

    def getPackageName(self):
        return self.package[CD_CONTENT][:self.package[CD_CONTENT].find("-")]

    def getDescription(self):
        return self.description[CD_CONTENT]
