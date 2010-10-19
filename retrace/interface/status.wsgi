#!/usr/bin/python

# /usr/share/abrt-retrace/status.wsgi

import sys
sys.path = ["/usr/share/abrt-retrace"] + sys.path

from retrace import *

def application(environ, start_response):
    request = Request(environ)

    match = URL_PARSER.match(request.script_name)
    if not match:
        return response(start_response, "404 Not Found")

    task = match.group(1)

    if not os.path.isdir(CONFIG["WorkDir"] + "/" + task):
        return response(start_response, "404 Not Found")

    status = "PENDING"
    if os.path.isfile(CONFIG["WorkDir"] + "/" + task + "/log"):
        if os.path.isfile(CONFIG["WorkDir"] + "/" + task + "/backtrace"):
            status = "FINISHED_SUCCESS"
        else:
            status = "FINISHED_FAILURE"

    return response(start_response, "200 OK", "", [("X-Task-Status", status)])