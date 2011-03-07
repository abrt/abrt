#!/usr/bin/python

import os
import re
import ConfigParser
import random
import sqlite3
from webob import Request
from subprocess import *

REQUIRED_FILES = ["coredump", "executable", "package"]

DF_BIN = "/bin/df"
DU_BIN = "/usr/bin/du"
TAR_BIN = "/bin/tar"
XZ_BIN = "/usr/bin/xz"

TASKID_PARSER = re.compile("^.*/([0-9]+)/*$")
PACKAGE_PARSER = re.compile("^(.+)-([0-9]+(\.[0-9]+)*-[0-9]+)\.([^-]+)$")
DF_OUTPUT_PARSER = re.compile("^([^ ^\t]*)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+)[ \t]+([0-9]+%)[ \t]+(.*)$")
DU_OUTPUT_PARSER = re.compile("^([0-9]+)")
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
  "DeleteTaskAfter": 120,
  "LogDir": "/var/log/abrt-retrace",
  "RepoDir": "/var/cache/abrt-retrace",
  "SaveDir": "/var/spool/abrt-retrace",
  "WorkDir": "/tmp/abrt-retrace",
  "UseWorkDir": False,
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
            get = parser.getboolean
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

def dir_size(path):
    pipe = Popen([DU_BIN, "-s", path], stdout=PIPE).stdout
    for line in pipe.readlines():
        match = DU_OUTPUT_PARSER.match(line)
        if match:
            pipe.close()
            return 1024 * int(match.group(1))

    pipe.close()
    return 0

def unpacked_size(archive):
    pipe = Popen([XZ_BIN, "--list", "--robot", archive], stdout=PIPE).stdout
    for line in pipe.readlines():
        match = XZ_OUTPUT_PARSER.match(line)
        if match:
            pipe.close()
            return int(match.group(4))

    pipe.close()
    return None

def guess_arch(coredump_path):
    pipe = Popen(["file", coredump_path], stdout=PIPE).stdout
    output = pipe.read()
    pipe.close()

    if "x86-64" in output:
        return "x86_64"

    if "80386" in output:
        return "i386"

    return None

def gen_task_password(taskdir):
    generator = random.SystemRandom()
    taskpass = ""
    for j in xrange(CONFIG["TaskPassLength"]):
        taskpass += generator.choice(TASKPASS_ALPHABET)

    try:
        passfile = open("%s/password" % taskdir, "w")
        passfile.write(taskpass)
        passfile.close()
    except:
        return None

    return taskpass

def get_task_est_time(taskdir):
    return 180

def new_task():
    i = 0
    newdir = CONFIG["SaveDir"]
    while os.path.exists(newdir) and i < 50:
        i += 1
        taskid = random.randint(pow(10, CONFIG["TaskIdLength"] - 1), pow(10, CONFIG["TaskIdLength"]) - 1)
        newdir = "%s/%d" % (CONFIG["SaveDir"], taskid)

    try:
        os.mkdir(newdir)
        taskpass = gen_task_password(newdir)
        if not taskpass:
            Popen(["rm", "-rf", newdir])
            raise Exception

        return taskid, taskpass, newdir
    except:
        return None, None, None

def unpack(archive):
    pipe = Popen([TAR_BIN, "xJf", archive])
    pipe.wait()
    return pipe.returncode

def response(start_response, status, body="", extra_headers=[]):
    start_response(status, [("Content-Type", "text/plain"), ("Content-Length", "%d" % len(body))] + extra_headers)
    return [body]

def get_active_tasks():
    tasks = []
    if CONFIG["UseWorkDir"]:
        tasksdir = CONFIG["WorkDir"]
    else:
        tasksdir = CONFIG["SaveDir"]

    for filename in os.listdir(tasksdir):
        if len(filename) != CONFIG["TaskIdLength"]:
            continue

        try:
            taskid = int(filename)
        except:
            continue

        path = "%s/%s" % (tasksdir, filename)
        if os.path.isdir(path) and not os.path.isfile("%s/retrace_log" % path):
            tasks.append(taskid)

    return tasks

def init_crashstats_db():
    try:
        con = sqlite3.connect("%s/%s" % (CONFIG["SaveDir"], CONFIG["DBFile"]))
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
            postrunning TINYINT NOT NULL,
            chrootsize BIGINT NOT NULL
          )
        """)
        con.commit()
        con.close()

        return True
    except:
        return False

def save_crashstats(crashstats):
    try:
        con = sqlite3.connect("%s/%s" % (CONFIG["SaveDir"], CONFIG["DBFile"]))
        query = con.cursor()
        query.execute("""
          INSERT INTO retracestats(taskid, package, version, release, arch,
          starttime, duration, prerunning, postrunning, chrootsize)
          VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
          """,
          (crashstats["taskid"], crashstats["package"], crashstats["version"],
           crashstats["release"], crashstats["arch"], crashstats["starttime"],
           crashstats["duration"], crashstats["prerunning"],
           crashstats["postrunning"], crashstats["chrootsize"])
        )
        con.commit()
        con.close()

        return True
    except:
        return False

class logger():
    def __init__(self, taskid):
        "Starts logging into savedir."
        self._logfile = open("%s/%s/log" % (CONFIG["SaveDir"], taskid), "w")

    def write(self, msg):
        "Writes msg into log file."
        if not self._logfile.closed:
            self._logfile.write(msg)
            self._logfile.flush()

    def close(self):
        "Finishes logging and renames file to retrace_log."
        if not self._logfile.closed:
            self._logfile.close()
            os.rename(self._logfile.name, self._logfile.name.replace("/log", "/retrace_log"))

### read config on import ###
read_config()
