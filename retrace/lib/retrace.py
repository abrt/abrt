#!/usr/bin/python
import os
import re
import ConfigParser
import random
from webob import Request
from subprocess import *

REQUIRED_FILES = ["architecture", "coredump", "packages", "release"]

DF_OUTPUT_PARSER = re.compile("^([^ ^\t]*)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+%)[ \t]+(.*)$")
XZ_OUTPUT_PARSER = re.compile("^totals[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+\.[0-9]+)[ \t]+([^ ^\t]+)[ \t]+([0-9]+)$")
URL_PARSER = re.compile("^/([0-9]+)/")

TASKPASS_ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

CONFIG_FILE = "/etc/abrt/retrace.conf"
CONFIG = {
    "TaskPassLength": 32,
    "MaxParallelTasks": 2,
    "MaxPackedSize": 30,
    "MaxUnpackedSize": 600,
    "MinStorageLeft": 10240,
    "WorkDir": "/var/spool/abrt-retrace",
}

def read_config():
    parser = ConfigParser.ConfigParser()
    parser.read(CONFIG_FILE)
    for key in CONFIG.keys():
        vartype = type(CONFIG[key])
        if vartype is int:
            get = parser.getint
        elif vartype is bool:
            get = parser.getbool
        elif vartype is float:
            get = parser.getfloat
        else:
            get = parser.get

        try:
            CONFIG[key] = get("retrace", key)
        except:
            pass

def free_space(path):
    pipe = Popen(["df", path], stdout=PIPE).stdout
    for line in pipe.readlines():
        match = DF_OUTPUT_PARSER.match(line)
        if match:
            pipe.close()
            return 1024 * int(match.group(4))

    pipe.close()
    return None

def unpacked_size(archive):
    pipe = Popen(["/usr/bin/xz", "--list", "--robot", archive], stdout=PIPE).stdout
    for line in pipe.readlines():
        match = XZ_OUTPUT_PARSER.match(line)
        if match:
            pipe.close()
            return int(match.group(4))

    pipe.close()
    return None

def gen_task_password(taskdir):
    taskpass = ""
    for j in xrange(CONFIG["TaskPassLength"]):
        taskpass += random.choice(TASKPASS_ALPHABET)

    try:
        passfile = open(taskdir + "/password", "w")
        passfile.write(taskpass)
        passfile.close()
    except:
        return None

    return taskpass

def new_task():
    i = 0
    newdir = CONFIG["WorkDir"]
    while os.path.exists(newdir) and i < 50:
        i += 1
        taskid = random.randint(100000000, 999999999)
        newdir = CONFIG["WorkDir"] + "/" + str(taskid)

    try:
        os.mkdir(newdir)
        taskpass = gen_task_password(newdir)
        if not taskpass:
            os.system("rm -rf " + newdir)
            raise

        return taskid, taskpass, newdir
    except:
        return None, None, None

def unpack(archive):
    pipe = Popen(["tar", "xJf", archive])
    pipe.wait()
    return pipe.returncode

def get_task_est_time(crashdir):
    return 180

def response(start_response, status, body="", extra_headers=[]):
    start_response(status, [("Content-Type", "text/plain"), ("Content-Length", str(len(body)))] + extra_headers)
    return [body]
