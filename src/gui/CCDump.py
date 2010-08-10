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

CD_UID          = "uid"
CD_UUID         = "UUID"
CD_INFORMALL    = "InformAll"
CD_DUPHASH      = "DUPHASH"
CD_DUMPDIR      = "DumpDir"
CD_COUNT        = "Count"
CD_REPORTED     = "Reported"
CD_MESSAGE      = "Message"

# FIXME - create method or smth that returns type|editable|content


class Dump():
    """Class for mapping the debug dump to python object"""
    not_required_fields = ["comment", "Message"]
    def __init__(self):
        # we set all attrs dynamically, so no need to have it in init
        for field in self.not_required_fields:
            self.__dict__[field] = None

    def __setattr__(self, name, value):
        if value != None:
            if name == "time":
                try:
                    self.__dict__["date"] = datetime.fromtimestamp(int(value[CD_CONTENT])).strftime("%c")
                except Exception, ex:
                    self.__dict__["date"] = value[CD_CONTENT]
                    log2("can't convert timestamp to date: %s" % ex)
            self.__dict__[name] = value[CD_CONTENT]
        else:
            self.__dict__[name] = value

    def __str__(self):
        return "Dump instance"

    def getUUID(self):
        return self.UUID

    def getUID(self):
        return self.uid

    def getCount(self):
        return int(self.Count)

    def getExecutable(self):
        return self.executable

    def getPackage(self):
        return self.package

    def isReported(self):
        return self.Reported == "1"

    def getMessage(self):
        if not self.Message:
            return "" #[]
        #return self.Message[CD_CONTENT].split('\n')
        return self.Message

    def getTime(self, fmt=None):
        if self.time:
            if fmt:
                try:
                    return datetime.fromtimestamp(int(self.time)).strftime(fmt)
                except Exception, ex:
                    log1(ex)
            return int(self.time)
        return self.time

    def getPackageName(self):
        name_delimiter_pos = self.package[:self.package.rfind("-")].rfind("-")
        # fix for kerneloops
        if name_delimiter_pos > 0:
            return self.package[:name_delimiter_pos]
        return self.package

    def getDescription(self):
        return self.description

    def getAnalyzerName(self):
        return self.analyzer

    def get_release(self):
        return self.release

    def get_reason(self):
        return self.reason

    def get_comment(self):
        return self.comment

    def get_component(self):
        return self.component

    def get_cmdline(self):
        return self.cmdline

    def get_arch(self):
        return self.architecture

    def get_kernel(self):
        return self.kernel

    def get_backtrace(self):
        try:
            return self.backtrace
        except AttributeError:
            return None

    def get_rating(self):
        try:
            return self.rating
        except AttributeError:
            return None

    def get_hostname(self):
        try:
            return self.hostname
        except AttributeError:
            return None
