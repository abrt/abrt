#!/usr/bin/python
import sys
import shutil
from retrace import *

PATH_PARSER = re.compile("^.+/([a-zA-Z0-9]+)/*$")
RELEASE_PARSERS = {
  "fedora": re.compile("^Fedora[^0-9]+([0-9]+)[^\(]\(([^\)]+)\)$"),
  "rhel": re.compile("^$"), #ToDo
}

def get_crashid(path):
    """
    Gets crash ID from directory path
    """
    match = PATH_PARSER.match(path);
    if match:
        return match.group(1)

    return None

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print "Usage: " + sys.argv[0] + " retrace_directory"
        sys.exit(1)

    workdir = sys.argv[1]

    if not os.path.isdir(workdir):
        print workdir + " is not a directory"
        sys.exit(2)

    crashid = get_crashid(workdir)
    if not crashid:
        print workdir + " is not a valid retrace server directory"
        sys.exit(3)

    for required_file in REQUIRED_FILES:
        if not os.path.isfile(workdir + "/crash/" + required_file):
            print workdir + " does not contain required file \"" + required_file + "\""
            sys.exit(4)

    try:
        arch_file = open(workdir + "/crash/architecture", "r")
        arch = repoarch = arch_file.read()
        arch_file.close()
    except:
        print "Unable to read architecture from \"architecture\" file"
        sys.exit(5)

    # required hack for public repos
    if arch == "i686":
        repoarch = "i386"

    try:
        release_file = open(workdir + "/crash/release", "r")
        release = release_file.read()
        release_file.close()
    except:
        print "Unable to read distribution and version from \"release\" file"
        sys.exit(6)

    version = distribution = None
    for distro in RELEASE_PARSERS.keys():
        match = RELEASE_PARSERS[distro].match(release)
        if match:
            version = match.group(1)
            distribution = distro
            break

    if not version or not distribution:
        print "Release \"" + release + "\" is not supported"
        sys.exit(7)

    packages = ""
    try:
        packages_file = open(workdir + "/crash/packages", "r")
        for package in packages_file.readlines():
            packages += " " + package.rstrip("\n")
        packages_file.close()
    except:
        print "Unable to read package list from \"packages\" file"
        sys.exit(8)


    try:
        mockcfg = open(workdir + "/mock.cfg", "w")
        mockcfg.write("config_opts['root'] = 'chroot'\n")
        mockcfg.write("config_opts['target_arch'] = '" + arch + "'\n")
        mockcfg.write("config_opts['chroot_setup_cmd'] = ' install" + packages + " shadow-utils yum-utils abrt-addon-ccpp gdb; debuginfo-install -y" + packages + "'\n")
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
        """
        mockcfg.write("\n")
        mockcfg.write("\"\"\"\n")
        mockcfg.close()
    except:
        print "Unable to create mock config file"
        sys.exit(9)

    mockr = "../../" + workdir + "/mock"

    print "Starting retrace"
    print "Initializing virtual root...",
    process = Popen(["mock", "init", "-r", mockr], stdout = PIPE, stderr = STDOUT)
    process.wait()
    output = process.stdout.read()
    process.stdout.close()

    if process.returncode != 0:
        print "Error"
        print "--- OUTPUT ---"
        print output
        sys.exit(10)

    print "OK"

    # installing debuginfos hidden into mock init command
    # subprocess module proclaims the command too long and fails
    """
    print "Installing debuginfos...",

    cmd = ["mock", "shell", "-r", mockr, "'debuginfo-install"]
    cmd.extend(packages.split(" "))
    cmd.append("'")

    process = Popen(cmd, stdout = PIPE, stderr = STDOUT)
    process.wait()
    output = process.stdout.read()
    process.stdout.close()

    if process.returncode != 0:
        print "Error"
        print "--- OUTPUT ---"
        print output
        sys.exit(11)

    print "OK"
    """

    print "Generating backtrace...",

    process = Popen(["cp", "-r", workdir + "/crash", workdir + "/chroot/root/var/spool/abrt"], stdout = PIPE, stderr = STDOUT)
    process.wait()
    output = process.stdout.read()
    process.stdout.close()

    if process.returncode != 0:
        print "Error"
        print "--- OUTPUT ---"
        print output
        sys.exit(12)

    process = Popen(["mock", "shell", "-r", mockr, "'abrt-action-generate-backtrace -d /var/spool/abrt/crash'"], stdout = PIPE, stderr = STDOUT)
    process.wait()
    output = process.stdout.read()
    process.stdout.close()

    if process.returncode != 0:
        print "Error"
        print "--- OUTPUT ---"
        print output
        sys.exit(13)

    process = Popen(["cp", workdir + "/chroot/root/var/spool/abrt/crash/backtrace", workdir], stdout = PIPE, stderr = STDOUT)
    process.wait()
    output = process.stdout.read()
    process.stdout.close()

    if process.returncode != 0:
        print "Error"
        print "--- OUTPUT ---"
        print output
        sys.exit(14)

    print "OK"
    print "Cleaning virtual root...",

    process = Popen(["mock", "clean", "-r", mockr], stdout = PIPE, stderr = STDOUT)
    process.wait()
    output = process.stdout.read()
    process.stdout.close()

    if process.returncode != 0:
        print "Error"
        print "--- OUTPUT ---"
        print output
        sys.exit(15)

    print "OK"
    print "Retrace successful"
