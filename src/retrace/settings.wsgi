#!/usr/bin/python

import sys
sys.path = ["/usr/share/abrt-retrace"] + sys.path

from retrace import *

def application(environ, start_response):
    formats = ""
    for format in HANDLE_ARCHIVE.keys():
        formats += " %s" % format

    output = [
               "running_tasks %d" % len(get_active_tasks()),
               "max_running_tasks %d" % CONFIG["MaxParallelTasks"],
               "max_packed_size %d" % CONFIG["MaxPackedSize"],
               "max_unpacked_size %d" % CONFIG["MaxUnpackedSize"],
               "supported_formats%s" % formats,
             ]

    return response(start_response, "200 OK", "\n".join(output))
