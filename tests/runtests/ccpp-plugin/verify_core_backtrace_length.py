#!/usr/bin/python3

import json
import sys

with open(sys.argv[1], 'r') as fh:
    core_backtrace = json.load(fh)

threads = core_backtrace['stacktrace']
trace = threads[0]['frames']

# Check that it is not longer than our limit
print(len(trace))
assert len(trace) <= 256

# Check that we have the bottom, i.e. main
for (i, frame) in enumerate(trace):
    if frame['function_name'] == 'main':
        break
else:
    print("main() not found in the trace")
    assert False

print('OK')
