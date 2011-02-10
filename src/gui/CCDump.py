# -*- coding: utf-8 -*-
from datetime import datetime

from abrt_utils import _, init_logging, log, log1, log2

# Keep in sync with [abrt_]crash_data.h!
CD_TYPE     = 0
CD_EDITABLE = 1
CD_CONTENT  = 2

CD_SYS = "s"
CD_BIN = "b"
CD_TXT = "t"

FILENAME_ANALYZER     = "analyzer"
FILENAME_EXECUTABLE   = "executable"
FILENAME_BINARY       = "binary"
FILENAME_CMDLINE      = "cmdline"
FILENAME_REASON       = "reason"
FILENAME_COREDUMP     = "coredump"
FILENAME_BACKTRACE    = "backtrace"
FILENAME_MEMORYMAP    = "memorymap"
FILENAME_DUPHASH      = "global_uuid"
FILENAME_CRASH_FUNCTION = "crash_function"
FILENAME_ARCHITECTURE = "architecture"
FILENAME_KERNEL       = "kernel"
FILENAME_TIME         = "time"
FILENAME_OS_RELEASE   = "os_release"
FILENAME_PACKAGE      = "package"
FILENAME_COMPONENT    = "component"
FILENAME_COMMENT      = "comment"
FILENAME_REPRODUCE    = "reproduce"
FILENAME_RATING       = "rating"
FILENAME_HOSTNAME     = "hostname"
FILENAME_REMOTE       = "remote"

FILENAME_UID          = "uid"
FILENAME_UUID         = "uuid"
FILENAME_INFORMALL    = "inform_all_users"
FILENAME_COUNT        = "count"
FILENAME_MESSAGE      = "message"

CD_DUMPDIR      = "DumpDir"
CD_EVENTS       = "Events"

# FIXME - create method or smth that returns type|editable|content

#EVENTS PARSING
REPORT_EVENT = "report"
REPORT_EVENT_PREFIX = "report_"

class Dump():
    """Class for mapping the debug dump to python object"""
    not_required_fields = [FILENAME_COMMENT, FILENAME_MESSAGE]
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
        return self.uuid

    def getUID(self):
        return self.uid

    def getDumpDir(self):
        return self.DumpDir

    def getCount(self):
        return int(self.count)

    def getExecutable(self):
        try:
            return self.executable
        except AttributeError, err:
            # try to rethrow the exception with the directory path, as
            # it's useful to know it in this case
            if "DumpDir" in self.__dict__:
                raise AttributeError("{0} (dump directory: {1})".format(str(err), self.DumpDir))
            else:
                raise

    def getPackage(self):
        return self.package

    def getMessage(self):
        if not self.message:
            return "" #[]
        #return self.message[CD_CONTENT].split('\n')
        return self.message

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

    def get_report_event_names(self):
        try:
            return [x for x in self.Events.split('\n') if (x == REPORT_EVENT or x[:len(REPORT_EVENT_PREFIX)] == REPORT_EVENT_PREFIX)]
        except AttributeError:
            return []
