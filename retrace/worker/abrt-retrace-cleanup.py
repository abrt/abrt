#!/usr/bin/python

import os
import shutil
import sys
import time
from retrace import *

if __name__ == "__main__":
    now = int(time.time())

    logfile = "%s/cleanup.log" % CONFIG["LogDir"]

    try:
        log = open(logfile, "a")
        log.write(time.strftime("[%Y-%m-%d %H:%M:%S] Running cleanup\n"))

        files = os.listdir(CONFIG["WorkDir"])
    except IOError, ex:
        print "Error opening log file: %s" % ex
        sys.exit(1)
    except OSError, ex:
        log.write("Unable to list work directory: %s" % ex)
        sys.exit(2)

    for filename in files:
        filepath = "%s/%s" % (CONFIG["WorkDir"], filename)
        if os.path.isdir(filepath):
            try:
                if (now - os.path.getctime(filepath)) / 3600 >= CONFIG["DeleteTaskAfter"]:
                    log.write("Deleting directory '%s'\n" % filepath)
                    shutil.rmtree(filepath)
            except OSError, ex:
                log.write("Error deleting directory: %s\n" % (filepath, ex))
            except IOError, ex:
                print "Unable to write to log file: %s" % ex
                sys.exit(3)

    try:
        log.close()
    except IOError, ex:
        print "Error closing log file: %s" % ex
        sys.exit(4)
