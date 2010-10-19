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

    task = match.group(1)

    logpath = CONFIG["WorkDir"] + "/" + task + "/log"
    if not os.path.isfile(logpath):
        return response(start_response, "404 Not Found")

    try:
        logfile = open(logpath, "r")
        output = logfile.read()
        logfile.close()
    except:
        response(start_response, "500 Internal Server Error", "Unable to read log file at server")

    return response(start_response, "200 OK", output)