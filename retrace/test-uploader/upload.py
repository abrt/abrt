#!/usr/bin/python
import os
import socket
import ssl
import sys

from subprocess import *

REQUIRED_FILES = ["analyzer", "architecture", "coredump", "executable", "package", "release"]
ARCHIVE_NAME = "crash.tar.xz"
ARCHIVE_CMD = ["tar", "cJf", ARCHIVE_NAME, "packages"] + REQUIRED_FILES
CONTENT_TYPE = "application/x-xz-compressed-tar"

if __name__ == "__main__":
    argc = len(sys.argv)
    if argc < 2 or argc > 3:
        print "ABRT Retrace Uploader"
        print "For test purposes only"
        print "Usage: '" + sys.argv[0] + " crash_directory [server_address]'"
        print "  Crash directory is the directory created by ABRT (default /var/spool/abrt/crash_directory/)."
        print "  Crash directory must contain analyzer, architecture, coredump, executable, package and release files."
        print "  If no server address is specified, default testing machine is used."
        print "  Only binary crashes (caught by CCpp) need retrace."
        print "  Only i?86 architecture is supported at the moment."
        print "  Root permissions are required to run the script (because of creating packages file)."
        print "    This is only required for testing. Future ABRT plugin will be able to handle everything itself."
        print "  The script shows raw HTTP output including X-Task-Id and X-Task-Password headers."
        print "    The tester is supposed to know what he's uploading and should handle task id and password on his own."
        sys.exit(1)

    if argc == 3:
        server_addr = sys.argv[2]
    else:
        server_addr = "simona.expresmu.sk"

    crashdir = sys.argv[1]

    print "Checking permissions...",
    sys.stdout.flush()

    if os.geteuid() != 0:
        print "Error"
        sys.exit(2)

    print "OK"
    print "Checking crash directory...",
    sys.stdout.flush()

    for required_file in REQUIRED_FILES:
        if not os.path.isfile(crashdir + "/" + required_file):
            print "Error"
            sys.exit(3)

    print "OK"
    print "Checking analyzer...",
    sys.stdout.flush()

    try:
        anfile = open(crashdir + "/analyzer", "r")
        an = anfile.read()
        anfile.close()
        if an != "CCpp":
            raise
    except:
        print "Error"
        sys.exit(4)

    print "OK"
    print "Checking architecture...",
    sys.stdout.flush()

    try:
        archfile = open(crashdir + "/architecture", "r")
        arch = archfile.read()
        archfile.close()
        if not arch in ["i386", "i586", "i686"]:
            raise
    except:
        print "Error"
        sys.exit(5)

    print "OK"
    print "Generating packages file...",
    sys.stdout.flush()

    try:
        gendeps = Popen(["./packages.py", crashdir], stdout = PIPE)
        gendeps.wait()
        if gendeps.returncode != 0:
            raise

        deps = gendeps.stdout.read()
        gendeps.stdout.close()

        packages = open(crashdir + "/packages", "w")
        packages.write(deps)
        packages.close()
    except:
        print "Error"
        sys.exit(6)

    print "OK"
    print "Compressing crash into .tar.xz archive...",
    sys.stdout.flush()

    os.chdir(crashdir)
    compress = Popen(ARCHIVE_CMD)
    compress.wait()
    if compress.returncode != 0:
        print "Error"
        sys.exit(7)

    print "OK"
    print "Building request...",
    sys.stdout.flush()

    try:
        f = open(crashdir + "/crash.tar.xz", "rb")
        data = f.read()
        f.close()
        request = "POST /create HTTP/1.1\r\n" \
          + "Host: " + server_addr + "\r\n" \
          + "Content-Type: " + CONTENT_TYPE + "\r\n" \
          + "Content-Length: " + str(len(data)) + "\r\n" \
          + "Connection: close\r\n" \
          + "\r\n"

        print "OK"
        print "---------"
        print request + "[raw data]"
        print "---------"
    except:
        print "Error"
        sys.exit(8)

    print "Connecting to retrace server @ ssl://" + server_addr + ":443...",
    sys.stdout.flush()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sslsock = ssl.wrap_socket(sock)
    try:
        sslsock.connect((server_addr, 443))
        print "OK"
        print "Sending request...",
        sys.stdout.flush()

        sslsock.write(request + data)

        print "OK"
        print "Receiving response...",
        sys.stdout.flush()

        response = ""
        block = sslsock.read()
        while (block):
            response += block
            block = sslsock.read()

        sslsock.close()

        print "OK"
        print "----------"
        print response,
        print "----------"
    except:
        print "Error"
        sys.exit(9)

    print "Cleanup...",
    sys.stdout.flush()

    try:
        os.unlink(ARCHIVE_NAME)
    except:
        print "Error"
        sys.exit(10)

    print "OK"
