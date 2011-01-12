#!/usr/bin/python
import os
import socket
import ssl
import sys
import tempfile

from subprocess import *

REQUIRED_FILES = ["analyzer", "architecture", "coredump", "executable", "package", "release", "uid"]
CONTENT_TYPE = "application/x-xz-compressed-tar"

if __name__ == "__main__":
    argc = len(sys.argv)
    if argc < 2 or argc > 3:
        print "ABRT Retrace Uploader"
        print "For test purposes only"
        print "Usage: '" + sys.argv[0] + " crash_directory server_address'"
        print "  Crash directory is the directory created by ABRT (default /var/spool/abrt/crash_directory/)."
        print "  Crash directory must contain analyzer, architecture, coredump, executable, package, release and uid files."
        print "  If no server address is specified, default testing machine is used."
        print "  Only binary crashes (caught by CCpp) need retrace."
        print "  The script shows raw HTTP output including X-Task-Id and X-Task-Password headers."
        print "    The tester is supposed to know what he's uploading and should handle task id and password on his own."
        sys.exit(1)

    crashdir = sys.argv[1]
    if argc == 2:
        server_addr = "retrace01.fedoraproject.org"
    else:
        server_addr = sys.argv[2]

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

    print "OK"

    print "Compressing crash into .tar.xz archive...",
    sys.stdout.flush()

    archive = tempfile.NamedTemporaryFile(delete = False, suffix = ".tar.xz")
    archive.close()

    os.chdir(crashdir)
    compress = Popen(["tar", "cJf", archive.name] + REQUIRED_FILES)
    compress.wait()
    if compress.returncode != 0:
        print "Error"
        sys.exit(7)

    print "OK"
    print "Building request...",
    sys.stdout.flush()

    try:
        f = open(archive.name, "rb")
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
        os.unlink(archive.name)
    except:
        print "Error"
        sys.exit(10)

    print "OK"
