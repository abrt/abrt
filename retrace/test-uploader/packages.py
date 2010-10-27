#!/usr/bin/python

############################################################
# packages.py crash_directory                              #
#                                                          #
# Generates list of packages needed for coredump analysis. #
# Appends unpackaged libraries (e.g. flashplayer).         #
# Outputs to stdout.                                       #
############################################################

import re
import sys
import os
from subprocess import *

LIB_RE = re.compile("^/.*/[^/]+\.so[\.0-9]*$")

def usage():
    print "Usage: " + sys.argv[0] + " crash_directory"
    print "    Crash directory must contain coredump and package files."

def get_libs(coredump):
    libs = []
    pipe = Popen(["strings", coredump], stdout = PIPE).stdout
    for line in pipe.readlines():
        match = LIB_RE.match(line)
        if match:
            lib = line.rstrip("\n")
            if not lib in libs:
                libs.append(lib)

    pipe.close()
    return libs

def get_package(lib):
    pipe = Popen(["rpm", "-qf", lib], stdout = PIPE)
    pipe.wait()
    if pipe.returncode != 0:
        return None

    package = pipe.stdout.read().rstrip("\n")
    pipe.stdout.close()
    return package

if __name__ == "__main__":
    if len(sys.argv) != 2:
        usage()
        sys.exit(1)

    if not os.path.isdir(sys.argv[1]):
        usage()
        sys.exit(2)

    coredump_file = sys.argv[1] + "/coredump"
    package_file = sys.argv[1] + "/package"
    if not os.path.isfile(coredump_file) or not os.path.isfile(package_file):
        usage()
        sys.exit(3)

    libs = get_libs(coredump_file)

    packages = []
    unpackaged = []

    for lib in libs:
        package = get_package(lib)
        if package:
            if not package in packages:
                packages.append(package)
        else:
            if not lib in unpackaged:
                unpackaged.append(lib)

    pkg = open(package_file, "r")
    crash_package = pkg.read()
    pkg.close()

    print crash_package
    for package in packages:
        print package
    print
    for lib in unpackaged:
        print lib
