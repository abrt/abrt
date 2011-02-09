#!/usr/bin/python

import sys
import time
from retrace import *

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
        print "Error %d" % errorcode
        print "--- OUTPUT ---"
        print output
        sys.exit(errorcode)

    return output

if __name__ == "__main__":
    starttime = time.time()

    if len(sys.argv) != 2:
        print "Usage: %s retrace_directory" % sys.argv[0]
        sys.exit(11)

    workdir = sys.argv[1]

    if not os.path.isdir(workdir):
        print "'%s' is not a directory" % workdir
        sys.exit(12)

    taskid_match = TASKID_PARSER.match(workdir)
    if not taskid_match:
        print "Unable to obtain task ID from given directory"
        sys.exit(13)
    else:
        taskid = taskid_match.group(1)

    # check the crash directory for required files
    for required_file in REQUIRED_FILES:
        if not os.path.isfile("%s/crash/%s" % (workdir, required_file)):
            print "Directory '%s' does not contain required file '%s'" % (workdir, required_file)
            sys.exit(14)

    # read architecture file
    try:
        arch_file = open("%s/crash/architecture" % workdir, "r")
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
        release_file = open("%s/crash/release" % workdir, "r")
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
        print "Release '%s' is not supported" % release
        sys.exit(17)

    # read package file
    try:
        package_file = open("%s/crash/package" % workdir, "r")
        crash_package = package_file.read()
        package_file.close()
    except:
        print "Unable to read crash package from 'package' file"
        sys.exit(18)

    # read required packages from coredump
    packages = "%s.%s" % (crash_package, arch)
    try:
        # ToDo: deal with not found build-ids
        pipe = Popen(["/usr/share/abrt-retrace/coredump2packages", "%s/crash/coredump" % workdir, "--repos=retrace-%s-%s-%s*" % (distribution, version, arch)], stdout=PIPE).stdout
        for line in pipe.readlines():
            if line == "\n":
                break

            packages += " %s" % line.rstrip("\n")

        pipe.close()
    except:
        print "Unable to obtain packages from 'coredump' file"
        sys.exit(19)

    # create mock config file
    try:
        mockcfg = open(workdir + "/mock.cfg", "w")
        mockcfg.write("config_opts['root'] = 'chroot'\n")
        mockcfg.write("config_opts['target_arch'] = '%s'\n" % arch)
        mockcfg.write("config_opts['chroot_setup_cmd'] = 'install %s shadow-utils abrt-addon-ccpp gdb'\n" % packages)
        mockcfg.write("config_opts['basedir'] = '%s'\n" % workdir)
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
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/%s-%s-%s/\n" % (distribution, version, arch))
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[fedora-debuginfo]\n")
        mockcfg.write("name=fedora-debuginfo\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/%s-%s-%s-debuginfo/\n" % (distribution, version, arch))
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates]\n")
        mockcfg.write("name=updates\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/%s-%s-%s-updates/\n" % (distribution, version, arch))
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates-debuginfo]\n")
        mockcfg.write("name=updates-debuginfo\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/%s-%s-%s-updates-debuginfo/\n" % (distribution, version, arch))
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates-testing]\n")
        mockcfg.write("name=updates-testing\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/%s-%s-%s-updates-testing/\n" % (distribution, version, arch))
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates-testing-debuginfo]\n")
        mockcfg.write("name=updates-testing-debuginfo\n")
        mockcfg.write("baseurl=file:///var/cache/abrt-retrace/%s-%s-%s-updates-testing-debuginfo/\n" % (distribution, version, arch))
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        # custom ABRT repo with ABRT 2.0 binaries - obsolete after release of ABRT 2.0
        mockcfg.write("[abrt]\n")
        mockcfg.write("name=abrt\n")
        mockcfg.write("baseurl=http://repos.fedorapeople.org/repos/mtoman/abrt20/%s-%s/%s/\n" % (distribution, version, repoarch))
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("\"\"\"\n")
        mockcfg.close()
    except Exception as ex:
        print "Unable to create mock config file: %s" % ex
        sys.exit(20)

    # get count of tasks running before starting
    prerunning = len(get_active_tasks()) - 1

    # run retrace
    mockr = "../../" + workdir + "/mock"

    print "Initializing virtual root...",

    retrace_run(21, ["mock", "init", "-r", mockr])
    retrace_run(22, ["mock", "-r", mockr, "--copyin", "%s/crash" % workdir, "/var/spool/abrt/crash"])
    retrace_run(23, ["touch", "%s/chroot/root/var/spool/abrt/crash/time" % workdir])

    print "OK"
    sys.stdout.flush()

    try:
        rootfile = open("%s/chroot/result/root.log" % workdir, "r")
        rootlog = rootfile.read()
        rootfile.close()
    except Exception as ex:
        print "Error reading root log: %s" % ex

    print "Generating backtrace...",

    retrace_run(24, ["mock", "shell", "-r", mockr, "--", "/usr/bin/abrt-action-generate-backtrace", "-d", "/var/spool/abrt/crash/"])
    retrace_run(25, ["mock", "-r", mockr, "--copyout", "/var/spool/abrt/crash/backtrace", workdir])
    retrace_run(26, ["chmod", "a+r", "%s/backtrace" % workdir])

    print "OK"
    sys.stdout.flush()

    chroot_size = dir_size("%s/chroot/root" % workdir)

    print "Cleaning up...",
    retrace_run(27, ["mock", "-r", mockr, "--scrub=all"])
    retrace_run(28, ["rm", "-rf", "%s/mock.cfg" % workdir, "%s/crash" % workdir])

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
      "chrootsize": chroot_size
    }

    print "Saving crash statistics...",

    if not init_crashstats_db() or not save_crashstats(crashstats):
        print "Error: %s" % crashstats
    else:
        print "OK"

    sys.stdout.flush()


    print
    print "=== ROOT LOG ==="
    print rootlog
