#!/usr/bin/python

import sys
import time
from retrace import *

sys.path = ["/usr/share/abrt-retrace/"] + sys.path
from plugins import *

LOG = None
taskid = None

def set_status(statusid):
    "Sets status for the task"
    if not LOG or not taskid:
        return

    filepath = "%s/%s/status" % (CONFIG["SaveDir"], taskid)
    try:
        statusfile = open(filepath, "w")
        statusfile.write(STATUS[statusid])
        statusfile.close()
    except:
        pass

    LOG.write("%s " %  STATUS[statusid])

def fail(exitcode):
    "Kills script with given exitcode"
    LOG.close()
    set_status(STATUS_FAIL)
    sys.exit(exitcode)

def retrace_run(errorcode, cmd):
    "Runs cmd using subprocess.Popen and kills script with errorcode on failure"
    try:
        process = Popen(cmd, stdout=PIPE, stderr=STDOUT)
        process.wait()
        output = process.stdout.read()
        process.stdout.close()
    except Exception as ex:
        process = None
        output = "An unhandled exception occured: %s" % ex

    if not process or process.returncode != 0:
        LOG.write("Error %d:\n=== OUTPUT ===\n%s\n" % (errorcode, output))
        fail(errorcode)

    return output

if __name__ == "__main__":
    starttime = time.time()

    if len(sys.argv) != 2:
        sys.stderr.write("Usage: %s task_id\n" % sys.argv[0])
        sys.exit(11)

    taskid = sys.argv[1]
    try:
        taskid_int = int(sys.argv[1])
    except:
        sys.stderr.write("Task ID may only contain digits.\n")
        sys.exit(12)

    savedir = workdir = "%s/%s" % (CONFIG["SaveDir"], taskid)

    if CONFIG["UseWorkDir"]:
        workdir = "%s/%s" % (CONFIG["WorkDir"], taskid)

    if not os.path.isdir(savedir):
        sys.stderr.write("Task '%s' does not exist.\n" % taskid)
        sys.exit(13)

    try:
        LOG = logger(taskid)
    except Exception as ex:
        sys.stderr.write("Unable to start logging for task '%s': %s.\n" % (taskid, ex))
        sys.exit(14)

    set_status(STATUS_ANALYZE)

    # check the crash directory for required files
    for required_file in REQUIRED_FILES:
        if not os.path.isfile("%s/crash/%s" % (savedir, required_file)):
            LOG.write("Crash directory does not contain required file '%s'.\n" % required_file)
            fail(15)

    # read architecture from coredump
    arch = guess_arch("%s/crash/coredump" % savedir)

    if not arch:
        LOG.write("Unable to read architecture from 'coredump' file.\n")
        fail(16)

    # read package file
    try:
        package_file = open("%s/crash/package" % savedir, "r")
        crash_package = package_file.read()
        package_file.close()
    except Exception as ex:
        LOG.write("Unable to read crash package from 'package' file: %s.\n" % ex)
        fail(17)

    # read release, distribution and version from release file
    release_path = "%s/crash/os_release" % savedir
    if not os.path.isfile(release_path):
        release_path = "%s/crash/release" % savedir

    try:
        release_file = open(release_path, "r")
        release = release_file.read()
        release_file.close()

        version = distribution = None
        for plugin in PLUGINS:
            match = plugin.abrtparser.match(release)
            if match:
                version = match.group(1)
                distribution = plugin.distribution
                break

        if not version or not distribution:
            raise Exception, "Release '%s' is not supported.\n" % release

    except Exception as ex:
        LOG.write("Unable to read distribution and version from 'release' file: %s.\n" % ex)
        LOG.write("Trying to guess distribution and version... ")
        distribution, version = guess_release(crash_package)
        if distribution and version:
            LOG.write("%s-%s\n" % (distribution, version))
        else:
            LOG.write("Failure\n")
            fail(18)

    # read package file
    try:
        package_file = open("%s/crash/package" % savedir, "r")
        crash_package = package_file.read()
        package_file.close()
    except Exception as ex:
        LOG.write("Unable to read crash package from 'package' file: %s.\n" % ex)
        fail(19)

    packages = crash_package

    # read required packages from coredump
    try:
        # ToDo: deal with not found build-ids
        pipe = Popen(["coredump2packages", "%s/crash/coredump" % savedir,
                      "--repos=retrace-%s-%s-%s*" % (distribution, version, arch)],
                     stdout=PIPE).stdout
        section = 0
        crash_package_or_component = None
        for line in pipe.readlines():
            if line == "\n":
                section += 1
                continue
            elif 0 == section:
                crash_package_or_component = line.strip()
            elif 1 == section:
                packages += " %s" % line.rstrip("\n")
            elif 2 == section:
                # Missing build ids
                pass
        pipe.close()
    except Exception as ex:
        LOG.write("Unable to obtain packages from 'coredump' file: %s.\n" % ex)
        fail(20)

    # create mock config file
    try:
        mockcfg = open("%s/mock.cfg" % savedir, "w")
        mockcfg.write("config_opts['root'] = '%s'\n" % taskid)
        mockcfg.write("config_opts['target_arch'] = '%s'\n" % arch)
        mockcfg.write("config_opts['chroot_setup_cmd'] = '--skip-broken install %s shadow-utils gdb rpm'\n" % packages)
        mockcfg.write("config_opts['plugin_conf']['ccache_enable'] = False\n")
        mockcfg.write("config_opts['plugin_conf']['yum_cache_enable'] = False\n")
        mockcfg.write("config_opts['plugin_conf']['root_cache_enable'] = False\n")
        mockcfg.write("\n")
        mockcfg.write("config_opts['yum.conf'] = \"\"\"\n")
        mockcfg.write("[main]\n")
        mockcfg.write("cachedir=/var/cache/yum\n")
        mockcfg.write("debuglevel=1\n")
        mockcfg.write("reposdir=/dev/null\n")
        mockcfg.write("logfile=/var/log/yum.log\n")
        mockcfg.write("retries=20\n")
        mockcfg.write("obsoletes=1\n")
        mockcfg.write("gpgcheck=0\n")
        mockcfg.write("assumeyes=1\n")
        mockcfg.write("syslog_ident=mock\n")
        mockcfg.write("syslog_device=\n")
        mockcfg.write("\n")
        mockcfg.write("#repos\n")
        mockcfg.write("\n")
        mockcfg.write("[%s]\n" % distribution)
        mockcfg.write("name=%s\n" % distribution)
        mockcfg.write("baseurl=file://%s/%s-%s-%s/\n" % (CONFIG["RepoDir"], distribution, version, arch))
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\"\"\"\n")
        mockcfg.close()
    except Exception as ex:
        LOG.write("Unable to create mock config file: %s.\n" % ex)
        fail(21)

    LOG.write("OK\n")

    # get count of tasks running before starting
    prerunning = len(get_active_tasks()) - 1

    # run retrace
    mockr = "../../%s/mock" % savedir

    set_status(STATUS_INIT)

    retrace_run(25, ["mock", "init", "-r", mockr])
    retrace_run(26, ["mock", "-r", mockr, "--copyin", "%s/crash" % savedir, "/var/spool/abrt/crash"])
    retrace_run(27, ["mock", "-r", mockr, "shell", "--", "chgrp", "-R", "mockbuild", "/var/spool/abrt/crash"])

    LOG.write("OK\n")

    # generate backtrace
    set_status(STATUS_BACKTRACE)

    backtrace = run_gdb(savedir)

    if not backtrace:
        LOG.write("Error\n")
        fail(29)

    try:
        bt_file = open("%s/backtrace" % savedir, "w")
        bt_file.write(backtrace)
        bt_file.close()
    except Exception as ex:
        LOG.write("Error: %s.\n" % ex)
        fail(30)

    LOG.write("OK\n")

    chroot_size = dir_size("%s/chroot/root" % workdir)

    # clean up temporary data
    set_status(STATUS_CLEANUP)

    retrace_run(31, ["mock", "-r", mockr, "--scrub=all"])
    retrace_run(32, ["rm", "-rf", "%s/mock.cfg" % savedir, "%s/crash" % savedir])

    # ignore error: workdir = savedir => workdir is not empty
    if CONFIG["UseWorkDir"]:
        try:
            os.rmdir(workdir)
        except:
            pass

    LOG.write("OK\n")

    # save crash statistics
    set_status(STATUS_STATS)

    duration = int(time.time() - starttime)

    package_match = PACKAGE_PARSER.match(crash_package)
    if not package_match:
        package = crash_package
        version = "unknown"
        release = "unknown"
    else:
        package = package_match.group(1)
        version = package_match.group(2)
        release = package_match.group(4)

    crashstats = {
      "taskid": taskid_int,
      "package": package,
      "version": version,
      "release": release,
      "arch": arch,
      "starttime": int(starttime),
      "duration": duration,
      "prerunning": prerunning,
      "postrunning": len(get_active_tasks()) - 1,
      "chrootsize": chroot_size
    }

    if not init_crashstats_db() or not save_crashstats(crashstats):
        LOG.write("Error: %s\n" % crashstats)
    else:
        LOG.write("OK\n")

    # publish bactkrace and log
    set_status(STATUS_FINISHING)

    try:
        os.rename("%s/backtrace" % savedir, "%s/retrace_backtrace" % savedir)
    except Exception as ex:
        LOG.write("Error: %s\n" % ex)
        fail(35)

    LOG.write("OK\n")
    LOG.write("Retrace took %d seconds.\n" % duration)

    set_status(STATUS_SUCCESS)
    LOG.write("\n")
    LOG.close()
