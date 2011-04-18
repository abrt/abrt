#!/usr/bin/python

import sys
sys.path = ["/usr/share/abrt-retrace"] + sys.path

from retrace import *

def application(environ, start_response):
    request = Request(environ)

    match = URL_PARSER.match(request.script_name)
    if not match:
        return response(start_response, "404 Not Found")

    taskdir = "%s/%s" % (CONFIG["SaveDir"], match.group(1))

    if not os.path.isdir(taskdir):
        return response(start_response, "404 Not Found")

    pwdpath = "%s/password" % taskdir
    try:
        pwdfile = open(pwdpath, "r")
        pwd = pwdfile.read()
        pwdfile.close()
    except:
        return response(start_response, "500 Internal Server Error", "Unable to verify password")

    if not "X-Task-Password" in request.headers or request.headers["X-Task-Password"] != pwd:
        return response(start_response, "403 Forbidden")

    status = "PENDING"
    if os.path.isfile("%s/retrace_log" % taskdir):
        if os.path.isfile("%s/retrace_backtrace" % taskdir):
            status = "FINISHED_SUCCESS"
        else:
            status = "FINISHED_FAILURE"

    statusmsg = status
    try:
        statusfile = open("%s/status" % taskdir, "r")
        statusmsg = statusfile.read()
        statusfile.close()
    except:
        pass

    return response(start_response, "200 OK", statusmsg, [("X-Task-Status", status)])
