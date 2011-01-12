#!/usr/bin/python

import sys
sys.path = ["/usr/share/abrt-retrace"] + sys.path

from retrace import *

def application(environ, start_response):
    request = Request(environ)

    match = URL_PARSER.match(request.script_name)
    if not match:
        return response(start_response, "404 Not Found")

    taskdir = CONFIG["WorkDir"] + "/" + match.group(1)

    if not os.path.isdir(taskdir):
        return response(start_response, "404 Not Found")

    pwdpath = taskdir + "/password"
    try:
        if not os.path.isfile(pwdpath):
            raise Exception

        pwdfile = open(pwdpath, "r")
        pwd = pwdfile.read()
        pwdfile.close()
    except:
        return response(start_response, "500 Internal Server Error", "Unable to verify password")

    if not "X-Task-Password" in request.headers or request.headers["X-Task-Password"] != pwd:
        return response(start_response, "403 Forbidden")

    newpass = gen_task_password(taskdir)
    if not newpass:
        return response(start_response, "500 Internal Server Error", "Unable to generate new password")

    status = "PENDING"
    if os.path.isfile(taskdir + "/retrace_log"):
        if os.path.isfile(taskdir + "/retrace_backtrace"):
            status = "FINISHED_SUCCESS"
        else:
            status = "FINISHED_FAILURE"

    newpass = gen_task_password(taskdir)
    if not newpass:
        return response(start_response, "500 Internal Server Error", "Unable to generate new password")

    return response(start_response, "200 OK", status, [("X-Task-Status", status), ("X-Task-Password", newpass)])
