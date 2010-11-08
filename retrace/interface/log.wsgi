#!/usr/bin/python

# /usr/share/abrt-retrace/log.wsgi

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

    newpass = gen_task_password(taskdir)
    if not newpass:
        return response(start_response, "500 Internal Server Error", "Unable to generate new password")

    logpath = taskdir + "/retrace_log"
    if not os.path.isfile(logpath):
        return response(start_response, "404 Not Found")

    try:
        logfile = open(logpath, "r")
        output = logfile.read()
        logfile.close()
    except:
        return response(start_response, "500 Internal Server Error", "Unable to read log file at server")

    return response(start_response, "200 OK", output)
