#!/usr/bin/python

import json
import sys

with open(sys.argv[1], 'r') as fh:
    core_backtrace = json.load(fh)

threads = core_backtrace['stacktrace']
trace = threads[0]['frames']

for (i, frame) in enumerate(trace):
    for o in ['file_name', 'build_id', 'build_id_offset']:
        if not o in frame or not frame[o]:
            print("Frame '%s': empty or missing '%s'" % (frame['function_name'], o))
            sys.exit(1)

print 'OK'
