#!/usr/bin/python

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

    logpath = "%s/retrace_log" % taskdir
    if not os.path.isfile(logpath):
        return response(start_response, "404 Not Found")

    try:
        logfile = open(logpath, "r")
        output = logfile.read()
        logfile.close()
    except:
        return response(start_response, "500 Internal Server Error", "Unable to read log file at server")

    return response(start_response, "200 OK", output)
