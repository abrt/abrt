#!/usr/bin/python

from retrace import *

def application(environ, start_response):
    request = Request(environ)

    match = URL_PARSER.match(request.script_name)
    if not match:
        return response(start_response, "404 Not Found",
                        "Invalid URL")

    taskdir = "%s/%s" % (CONFIG["SaveDir"], match.group(1))

    if not os.path.isdir(taskdir):
        return response(start_response, "404 Not Found",
                        "There is no such task")

    pwdpath = "%s/password" % taskdir
    try:
        pwdfile = open(pwdpath, "r")
        pwd = pwdfile.read()
        pwdfile.close()
    except:
        return response(start_response, "500 Internal Server Error",
                        "Unable to verify password")

    if not "X-Task-Password" in request.headers or \
       request.headers["X-Task-Password"] != pwd:
        return response(start_response, "403 Forbidden",
                        "Invalid password")

    btpath = "%s/retrace_backtrace" % taskdir
    if not os.path.isfile(btpath):
        return response(start_response, "404 Not Found",
                        "There is no backtrace for the specified task")

    try:
        btfile = open(btpath, "r")
        output = btfile.read()
        btfile.close()
    except:
        return response(start_response, "500 Internal Server Error",
                        "Unable to read backtrace file")

    return response(start_response, "200 OK", output)
