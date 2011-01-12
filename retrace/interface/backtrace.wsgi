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
            raise

        pwdfile = open(pwdpath, "r")
        pwd = pwdfile.read()
        pwdfile.close()
    except:
        return response(start_response, "500 Internal Server Error", "Unable to verify password")

    if not "X-Task-Password" in request.headers or request.headers["X-Task-Password"] != pwd:
        return response(start_response, "403 Forbidden")

    btpath = taskdir + "/retrace_backtrace"
    if not os.path.isfile(btpath):
        return response(start_response, "404 Not Found")

    try:
        btfile = open(btpath, "r")
        output = btfile.read()
        btfile.close()
    except:
        return response(start_response, "500 Internal Server Error", "Unable to read backtrace file at server")

    newpass = gen_task_password(taskdir)
    if not newpass:
        return response(start_response, "500 Internal Server Error", "Unable to generate new password")

    return response(start_response, "200 OK", output, [("X-Task-Password", newpass)])
