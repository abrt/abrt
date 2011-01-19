#!/usr/bin/python

import sys
import time
from retrace import *

def retrace_run(errorcode, cmd):
    """
    Runs cmd using subprocess.Popen and kills script with errorcode on failure
    """
    try:
        process = Popen(cmd, stdout = PIPE, stderr = STDOUT)
        process.wait()
        output = process.stdout.read()
        process.stdout.close()
    except Exception as ex:
        extype, exvalue, extraceback = sys.exc_info()
        process = None
        output = "An unhandled exception " + str(extype) + " occured: " + str(exvalue)

    if not process or process.returncode != 0:
        print "Error"
        print "--- OUTPUT ---"
        print output
        sys.exit(errorcode)

    return output

if __name__ == "__main__":
    starttime = time.time()

    if len(sys.argv) != 2:
        print "Usage: " + sys.argv[0] + " retrace_directory"
        sys.exit(11)

    workdir = sys.argv[1]

    if not os.path.isdir(workdir):
        print workdir + " is not a directory"
        sys.exit(12)

    taskid_match = TASKID_PARSER.match(workdir)
    if not taskid_match:
        print "Unable to obtain task ID from given directory"
        sys.exit(13)
    else:
        taskid = taskid_match.group(1)

    # check the crash directory for required files
    for required_file in REQUIRED_FILES:
        if not os.path.isfile(workdir + "/crash/" + required_file):
            print "Directory '" + workdir + "' does not contain required file \"" + required_file + "\""
            sys.exit(14)

    # read architecture file
    try:
        arch_file = open(workdir + "/crash/architecture", "r")
        arch = repoarch = arch_file.read()
        arch_file.close()
    except:
        print "Unable to read architecture from 'architecture' file"
        sys.exit(15)

    # required hack for public repos
    if arch == "i686":
        repoarch = "i386"

    # read release, distribution and version from release file
    try:
        release_file = open(workdir + "/crash/release", "r")
        release = release_file.read()
        release_file.close()
    except:
        print "Unable to read distribution and version from 'release' file"
        sys.exit(16)

    version = distribution = None
    for distro in RELEASE_PARSERS.keys():
        match = RELEASE_PARSERS[distro].match(release)
        if match:
            version = match.group(1)
            distribution = distro
            break

    if not version or not distribution:
        print "Release '" + release + "' is not supported"
        sys.exit(17)

    # read package file
    try:
        package_file = open(workdir + "/crash/package", "r")
        crash_package = package_file.read()
        package_file.close()
    except:
        print "Unable to read crash package from 'package' file"
        sys.exit(18)

    # read required packages from coredump
    packages = crash_package + "." + arch
    try:
        # ToDo: deal with not found build-ids
        pipe = Popen(["/usr/share/abrt-retrace/coredump2packages.py", workdir + "/crash/coredump", "--repos=retrace-*"], stdout = PIPE).stdout
        for line in pipe.readlines():
            if line == "\n":
                break

            packages += " " + line.rstrip("\n")

        pipe.close()
    except:
        print "Unable to obtain packages from 'coredump' file"
        sys.exit(19)

    # create mock config file
    try:
        mockcfg = open(workdir + "/mock.cfg", "w")
        mockcfg.write("config_opts['root'] = '" + taskid + "'\n")
        mockcfg.write("config_opts['target_arch'] = '" + arch + "'\n")
        mockcfg.write("config_opts['chroot_setup_cmd'] = 'install " + packages + " shadow-utils abrt-addon-ccpp gdb'\n")
        mockcfg.write("config_opts['basedir'] = '" + workdir + "'\n")
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
        mockcfg.write("[fedora]\n")
        mockcfg.write("name=fedora\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/" + distribution + "-" + version + "-" + arch + "/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[fedora-debuginfo]\n")
        mockcfg.write("name=fedora-debuginfo\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/" + distribution + "-" + version + "-" + arch + "-debuginfo/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates]\n")
        mockcfg.write("name=updates\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/" + distribution + "-" + version + "-" + arch + "-updates/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates-debuginfo]\n")
        mockcfg.write("name=updates-debuginfo\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/" + distribution + "-" + version + "-" + arch + "-updates-debuginfo/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates-testing]\n")
        mockcfg.write("name=updates-testing\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/" + distribution + "-" + version + "-" + arch + "-updates-testing/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates-testing-debuginfo]\n")
        mockcfg.write("name=updates-testing-debuginfo\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/" + distribution + "-" + version + "-" + arch + "-updates-testing-debuginfo/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        # custom ABRT repo with ABRT 2.0 binaries - obsolete after release of ABRT 2.0
        mockcfg.write("[abrt]\n")
        mockcfg.write("name=abrt\n")
        mockcfg.write("baseurl=http://repos.fedorapeople.org/repos/mtoman/abrt20/" + distribution + "-" + version + "/" + repoarch + "/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("\"\"\"\n")
        mockcfg.close()
    except:
        print "Unable to create mock config file"
        sys.exit(20)

    # get count of tasks running before starting
    prerunning = len(get_active_tasks()) - 1

    # run retrace
    mockr = "../../" + workdir + "/mock"

    print "Initializing virtual root...",

    retrace_run(21, ["mock", "init", "-r", mockr])
    retrace_run(22, ["mock", "-r", mockr, "--copyin", workdir + "/crash", "/var/spool/abrt/crash"])

    print "OK"
    sys.stdout.flush()

    print "Generating backtrace...",

    retrace_run(24, ["mock", "shell", "-r", mockr, "--", "/usr/libexec/abrt-action-generate-backtrace -d /var/spool/abrt/crash"])
    retrace_run(25, ["mock", "-r", mockr, "--copyout", "/var/spool/abrt/crash/backtrace", workdir])
    retrace_run(26, ["chmod", "a+r", workdir + "/backtrace"])

    print "OK"
    sys.stdout.flush()

    print "Cleaning up...",
    retrace_run(27, ["mock", "-r", mockr, "--scrub=all"])
    retrace_run(28, ["rm", "-rf", workdir + "/mock.cfg", workdir + "/crash"])

    print "OK"
    sys.stdout.flush()

    # save crash statistics
    duration = int(time.time() - starttime)
    print "Retrace took %d seconds" % duration

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
      "taskid": int(taskid),
      "package": package,
      "version": version,
      "release": release,
      "arch": arch,
      "starttime": int(starttime),
      "duration": duration,
      "prerunning": prerunning,
      "postrunning": len(get_active_tasks()) - 1,
    }

    print "Saving crash statistics...",

    if not init_crashstats_db() or not save_crashstats(crashstats):
        print "Error"
    else:
        print "OK"

    sys.stdout.flush()
