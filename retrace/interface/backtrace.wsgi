#!/usr/bin/python

# /usr/share/abrt-retrace/backtrace.wsgi

import sys
sys.path = ["/usr/share/abrt-retrace"] + sys.path

from retrace import *

def application(environ, start_response):
    request = Request(environ)

    match = URL_PARSER.match(request.script_name)
    if not match:
        return response(start_response, "404 Not Found")

    task = match.group(1)

    btpath = CONFIG["WorkDir"] + "/" + task + "/backtrace"
    if not os.path.isfile(btpath):
        return response(start_response, "404 Not Found")

    try:
        btfile = open(btpath, "r")
        output = btfile.read()
        btfile.close()
    except:
        response(start_response, "500 Internal Server Error", "Unable to read backtrace file at server")

    return response(start_response, "200 OK", output)