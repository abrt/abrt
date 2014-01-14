#!/usr/bin/python

import json
import sys

expected = [
    'crash',
    'varargs',
    'f',
    'callback',
    'call_me_back',
    'recursive',
    'recursive',
    'recursive',
    'main'
]

with open(sys.argv[1], 'r') as fh:
    core_backtrace = json.load(fh)

assert core_backtrace['executable'] == '/usr/bin/will_segfault'
assert core_backtrace['signal'] == 11
threads = core_backtrace['stacktrace']
assert len(threads) == 1
assert threads[0]['crash_thread'] == True
trace = threads[0]['frames']

assert len(trace) == len(expected)

for (i, frame) in enumerate(trace):
    assert frame['function_name'] == expected[i]
    assert isinstance(frame['address'], int)
    assert isinstance(frame['build_id'], unicode)
    assert isinstance(frame['build_id_offset'], int)
    assert isinstance(frame['file_name'], unicode)
    assert ('will_segfault' in frame['file_name'] or 'libwillcrash' in frame['file_name'])

print 'OK'
