#!/usr/bin/python3

import json
import sys
import os

def assert_equals(current, expected):
    if str(expected) not in str(current):
        print("%s != %s" % (str(current), str(expected)))
        sys.exit(1)

def assert_isinstance(obj, typ):
    if not isinstance(obj, typ):
        print("%s isn't instance of %s" % (str(obj), str(typ)))
        sys.exit(1)

arch = sys.argv[2]
executable = sys.argv[3]

expected = {
'passwd' : [
    ('__select', ['libc']),
    ('_pam_await_timer', ['libpam.so']),
    ('pam_chauthtok', ['libpam.so']),
    ('main', ['passwd'])
    ],
'will_segfault' : [
    ('crash', ['will_segfault']),
    ('varargs', ['will_segfault']),
    ('f', ['will_segfault']),
    ('callback', ['will_segfault']),
    ('call_me_back', ['libwillcrash']),
    ('recursive', ['will_segfault']),
    ('recursive', ['will_segfault']),
    ('recursive', ['will_segfault']),
    ('main', ['will_segfault']),
    ],
'unprivileged_to_root' : [
    ('kill', ['libc']),
    ('main', ['unprivileged_to_root']),
    ],
'sleep' : [
    ('__nanosleep', ['libc']),
    ('rpl_nanosleep', ['sleep']),
    ('xnanosleep', ['sleep']),
    ('main', ['sleep'])
    ],
'will_segfault_in_new_pid' : [
    ('main', ['will_segfault_in_new_pid'])
    ],
'root_to_unprivileged' : [
    ('kill', ['libc']),
    ('main', ['root_to_unprivileged'])
    ]
}[os.path.basename(executable)]

if arch in ['ppc64','ppc64le']:
    expected.append(('generic_start_main.isra.0',['libc']))

try:
    with open(sys.argv[1], 'r') as fh:
        core_backtrace = json.load(fh)
except Exception as ex:
    print(str(ex))
    sys.exit(1)

assert_equals(core_backtrace['executable'], executable)
assert_equals(core_backtrace['signal'], 11)
threads = core_backtrace['stacktrace']
assert_equals(len(threads), 1)
assert_equals(threads[0]['crash_thread'], True)
trace = threads[0]['frames']

print("++++ core_backtrace.frames of %s:" % (executable))
for (i, frame) in enumerate(trace):
    print("Function:", frame['function_name'], "- file name:", frame['file_name'])
print("----")

assert_equals(len(trace), len(expected))

for (i, frame) in enumerate(trace):
    assert_equals(frame['function_name'], expected[i][0])
    assert_isinstance(frame['address'], int)
    assert_isinstance(frame['build_id'], str)
    assert_isinstance(frame['build_id_offset'], int)

    if not any(map(lambda k: k in frame['file_name'], expected[i][1])):
        print("%s not in %s" % (frame['file_name'], str(expected[i][1])))
        sys.exit(1)
