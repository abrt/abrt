#!/usr/bin/python

##############################################################################
# Usage:                                                                     #
# worker.py retrace_directory                                                #
#                                                                            #
# retrace_directory is the directory created by abrt-retrace-interface.      #
# By default it is "/var/spool/abrt-retrace/crashid".                        #
#                                                                            #
# worker.py is a script executing main part of the retrace process. It       #
# analyzes the crash in the given retrace directory, cleans up unneeded data #
# (crash directory and virtual chroot) and creates backtrace file.           #
#                                                                            #
# Created by:                                                                #
# Michal Toman <mtoman@redhat.com>                                           #
##############################################################################

import sys
import time
from retrace import *

RELEASE_PARSERS = {
  "fedora": re.compile("^Fedora[^0-9]+([0-9]+)[^\(]\(([^\)]+)\)$"),
  "rhel": re.compile("^$"), #ToDo
}

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

if __name__ == "__main__":
    starttime = time.time()

    if len(sys.argv) != 2:
        print "Usage: " + sys.argv[0] + " retrace_directory"
        sys.exit(1)

    workdir = sys.argv[1]

    if not os.path.isdir(workdir):
        print workdir + " is not a directory"
        sys.exit(2)

    for required_file in REQUIRED_FILES:
        if not os.path.isfile(workdir + "/crash/" + required_file):
            print "Directory '" + workdir + "' does not contain required file \"" + required_file + "\""
            sys.exit(3)

    try:
        arch_file = open(workdir + "/crash/architecture", "r")
        arch = repoarch = arch_file.read()
        arch_file.close()
    except:
        print "Unable to read architecture from 'architecture' file"
        sys.exit(4)

    # required hack for public repos
    if arch == "i686":
        repoarch = "i386"

    try:
        release_file = open(workdir + "/crash/release", "r")
        release = release_file.read()
        release_file.close()
    except:
        print "Unable to read distribution and version from 'release' file"
        sys.exit(5)

    version = distribution = None
    for distro in RELEASE_PARSERS.keys():
        match = RELEASE_PARSERS[distro].match(release)
        if match:
            version = match.group(1)
            distribution = distro
            break

    if not version or not distribution:
        print "Release '" + release + "' is not supported"
        sys.exit(6)

    packages = ""
    try:
        packages_file = open(workdir + "/crash/packages", "r")
        for package in packages_file.readlines():
            packages += " " + package.rstrip("\n")
        packages_file.close()
    except:
        print "Unable to read package list from \"packages\" file"
        sys.exit(7)

    chroot = distribution + "-" + version + "-" + arch

    try:
        mockcfg = open(workdir + "/mock.cfg", "w")
        mockcfg.write("config_opts['root'] = '" + chroot + "'\n")
        mockcfg.write("config_opts['target_arch'] = '" + arch + "'\n")
        mockcfg.write("config_opts['chroot_setup_cmd'] = 'install" + packages + " yum-utils shadow-utils abrt-addon-ccpp gdb'\n")
        mockcfg.write("config_opts['basedir'] = '" + workdir + "'\n")
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
        # at the moment only works for fedora with global repos
        # ToDo RHEL and local repos
        mockcfg.write("[fedora]\n")
        mockcfg.write("name=fedora\n")
        mockcfg.write("mirrorlist=https://mirrors.fedoraproject.org/metalink?repo=fedora-" + version + "&arch=" + repoarch + "\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[fedora-debuginfo]\n")
        mockcfg.write("name=fedora-debuginfo\n")
        mockcfg.write("mirrorlist=https://mirrors.fedoraproject.org/metalink?repo=fedora-debug-" + version + "&arch=" + repoarch + "\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates]\n")
        mockcfg.write("name=updates\n")
        mockcfg.write("mirrorlist=https://mirrors.fedoraproject.org/metalink?repo=updates-released-f" + version + "&arch=" + repoarch + "\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates-debuginfo]\n")
        mockcfg.write("name=updates-debuginfo\n")
        mockcfg.write("mirrorlist=https://mirrors.fedoraproject.org/metalink?repo=updates-released-debug-f" + version + "&arch=" + repoarch + "\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        # custom repo containing abrt-1.1.4.1, abrt-addon-ccpp-1.1.4.1 and abrt-libs-1.1.4.1 for fedora-[12|13|14]-[i686|x86_64]
        mockcfg.write("[abrt]\n")
        mockcfg.write("name=abrt\n")
        mockcfg.write("baseurl=http://simona.expresmu.sk:44480/" + distribution + "-" + version + "-" + arch + "-abrt\n")
        mockcfg.write("failovermethod=priority\n")
        """
        # testing local repos
        mockcfg.write("[fedora]\n")
        mockcfg.write("name=fedora\n")
        mockcfg.write("baseurl=http://localhost:44480/" + distribution + "-" + version + "-" + arch + "/Packages/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[fedora-debuginfo]\n")
        mockcfg.write("name=fedora-debuginfo\n")
        mockcfg.write("baseurl=http://localhost:44480/" + distribution + "-" + version + "-" + arch + "-debuginfo/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates]\n")
        mockcfg.write("name=updates\n")
        mockcfg.write("baseurl=http://localhost:44480/" + distribution + "-" + version + "-" + arch + "-updates/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[updates-debuginfo]\n")
        mockcfg.write("name=updates-debuginfo\n")
        mockcfg.write("baseurl=http://localhost:44480/" + distribution + "-" + version + "-" + arch + "-updates-debuginfo/\n")
        mockcfg.write("failovermethod=priority\n")
        mockcfg.write("\n")
        mockcfg.write("[abrt]\n")
        mockcfg.write("name=abrt\n")
        mockcfg.write("baseurl=http://localhost:44480/" + distribution + "-" + version + "-" + arch + "-abrt\n")
        mockcfg.write("failovermethod=priority\n")
        """
        mockcfg.write("\n")
        mockcfg.write("\"\"\"\n")
        mockcfg.close()
    except:
        print "Unable to create mock config file"
        sys.exit(8)

    mockr = "../../" + workdir + "/mock"

    print "Initializing virtual root...",
    sys.stdout.flush()

    retrace_run(11, ["mock", "init", "-r", mockr])

    print "OK"

    print "Installing debuginfos...",
    sys.stdout.flush()

    retrace_run(12, ["mock", "shell", "-r", mockr, "debuginfo-install" + packages])

    print "OK"

    print "Generating backtrace...",
    sys.stdout.flush()

    retrace_run(13, ["cp", "-r", workdir + "/crash", workdir + "/" + chroot + "/root/var/spool/abrt"])
    retrace_run(14, ["mock", "shell", "-r", mockr, "--", "abrt-action-generate-backtrace -d /var/spool/abrt/crash"])
    retrace_run(15, ["cp", workdir + "/" + chroot + "/root/var/spool/abrt/crash/backtrace", workdir])

    print "OK"
    print "Retrace took " + str(time.time() - starttime) + " seconds"
