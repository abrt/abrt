#!/usr/bin/python

import json
import sys
import os

expected = {
'/usr/bin/will_segfault' : [
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
}

with open(sys.argv[1], 'r') as fh:
    core_backtrace = json.load(fh)

assert (core_backtrace['executable'] in ['/usr/bin/will_segfault', '/usr/bin/will_stackoverflow'])

assert core_backtrace['signal'] == 11
threads = core_backtrace['stacktrace']
assert len(threads) == 1
assert threads[0]['crash_thread'] == True
trace = threads[0]['frames']

if core_backtrace['executable'] in expected:
    assert len(trace) == len(expected[core_backtrace['executable']])

for (i, frame) in enumerate(trace):
    if core_backtrace['executable'] in expected:
        assert frame['function_name'] == expected[core_backtrace['executable']][i]

    assert isinstance(frame['address'], int)
    assert isinstance(frame['build_id'], unicode)
    assert isinstance(frame['build_id_offset'], int)
    assert isinstance(frame['file_name'], unicode)
    assert (os.path.basename(core_backtrace['executable']) in frame['file_name']
        or 'libwillcrash' in frame['file_name']
        or 'libc' in frame['file_name'])

print 'OK'
