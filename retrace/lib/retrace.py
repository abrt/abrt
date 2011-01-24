#!/usr/bin/python

import os
import re
import ConfigParser
import random
import sqlite3
from webob import Request
from subprocess import *

REQUIRED_FILES = ["architecture", "coredump", "release"]

DF_BIN = "/bin/df"
TAR_BIN = "/bin/tar"
XZ_BIN = "/usr/bin/xz"

TASKID_PARSER = re.compile("^.*/([0-9]+)/*$")
PACKAGE_PARSER = re.compile("^(.+)-([0-9]+(\.[0-9]+)*-[0-9]+)\.([^-]+)$")
DF_OUTPUT_PARSER = re.compile("^([^ ^\t]*)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+%)[ \t]+(.*)$")
XZ_OUTPUT_PARSER = re.compile("^totals[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+\.[0-9]+)[ \t]+([^ ^\t]+)[ \t]+([0-9]+)")
URL_PARSER = re.compile("^/([0-9]+)/?")
RELEASE_PARSERS = {
  "fedora": re.compile("^Fedora[^0-9]+([0-9]+)[^\(]\(([^\)]+)\)$"),
}

TASKPASS_ALPHABET = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

CONFIG_FILE = "/etc/abrt/retrace.conf"
CONFIG = {
    "TaskIdLength": 9,
    "TaskPassLength": 32,
    "MaxParallelTasks": 10,
    "MaxPackedSize": 30,
    "MaxUnpackedSize": 600,
    "MinStorageLeft": 10240,
    "WorkDir": "/var/spool/abrt-retrace",
    "DBFile": "stats.db",
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
    pipe = Popen([DF_BIN, path], stdout=PIPE).stdout
    for line in pipe.readlines():
        match = DF_OUTPUT_PARSER.match(line)
        if match:
            pipe.close()
            return 1024 * int(match.group(4))

    pipe.close()
    return None

def unpacked_size(archive):
    pipe = Popen([XZ_BIN, "--list", "--robot", archive], stdout=PIPE).stdout
    for line in pipe.readlines():
        match = XZ_OUTPUT_PARSER.match(line)
        if match:
            pipe.close()
            return int(match.group(4))

    pipe.close()
    return None

def gen_task_password(taskdir):
    generator = random.SystemRandom()
    taskpass = ""
    for j in xrange(CONFIG["TaskPassLength"]):
        taskpass += generator.choice(TASKPASS_ALPHABET)

    try:
        passfile = open(taskdir + "/password", "w")
        passfile.write(taskpass)
        passfile.close()
    except:
        return None

    return taskpass

def get_task_est_time(taskdir):
    return 180

def new_task():
    i = 0
    newdir = CONFIG["WorkDir"]
    while os.path.exists(newdir) and i < 50:
        i += 1
        taskid = random.randint(pow(10, CONFIG["TaskIdLength"] - 1), pow(10, CONFIG["TaskIdLength"]) - 1)
        newdir = CONFIG["WorkDir"] + "/" + str(taskid)

    try:
        os.mkdir(newdir)
        taskpass = gen_task_password(newdir)
        if not taskpass:
            os.system("rm -rf " + newdir)
            raise Exception

        return taskid, taskpass, newdir
    except:
        return None, None, None

def unpack(archive):
    pipe = Popen([TAR_BIN, "xJf", archive])
    pipe.wait()
    return pipe.returncode

def response(start_response, status, body="", extra_headers=[]):
    start_response(status, [("Content-Type", "text/plain"), ("Content-Length", str(len(body)))] + extra_headers)
    return [body]

def get_active_tasks():
    tasks = []
    for filename in os.listdir(CONFIG["WorkDir"]):
        if len(filename) != 9:
            continue

        try:
            taskid = int(filename)
        except:
            continue

        path = "%s/%s" % (CONFIG["WorkDir"], filename)
        if os.path.isdir(path) and not os.path.isfile("%s/retrace_log" % path):
            tasks.append(taskid)

    return tasks

def init_crashstats_db():
    try:
        con = sqlite3.connect("%s/%s" % (CONFIG["WorkDir"], CONFIG["DBFile"]))
        query = con.cursor()
        query.execute("""
          CREATE TABLE IF NOT EXISTS
          retracestats(
            taskid INT NOT NULL,
            package VARCHAR(255) NOT NULL,
            version VARCHAR(16) NOT NULL,
            release VARCHAR(16) NOT NULL,
            arch VARCHAR(8) NOT NULL,
            starttime INT NOT NULL,
            duration INT NOT NULL,
            prerunning TINYINT NOT NULL,
            postrunning TINYINT NOT NULL
          )
        """)
        con.commit()
        con.close()

        return True
    except:
        return False

def save_crashstats(crashstats):
    try:
        con = sqlite3.connect("%s/%s" % (CONFIG["WorkDir"], CONFIG["DBFile"]))
        query = con.cursor()
        query.execute("""
          INSERT INTO retracestats(taskid, package, version, release, arch, starttime, duration,
          prerunning, postrunning) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)
          """,
          (crashstats["taskid"], crashstats["package"], crashstats["version"], crashstats["release"], crashstats["arch"], crashstats["starttime"], crashstats["duration"], crashstats["prerunning"], crashstats["postrunning"])
        )
        con.commit()
        con.close()

        return True
    except Exception, e:
        print e
        return False

### read config on import ###
read_config()
