#! /usr/bin/python -u
# -*- coding: utf-8 -*-

# WARNING: python -u means unbuffered I/O without it the messages are
# passed to the parent asynchronously which looks bad in clients..
from subprocess import Popen, PIPE
import sys
import os
import getopt

# everything was ok
RETURN_OK = 0
# serious problem, should be logged somewhere
RETURN_FAILURE = 2


GETTEXT_PROGNAME = "abrt"
import locale
import gettext

_ = lambda x: gettext.lgettext(x)

def init_gettext():
    try:
        locale.setlocale(locale.LC_ALL, "")
    except locale.Error:
        os.environ['LC_ALL'] = 'C'
        locale.setlocale(locale.LC_ALL, "")
    gettext.bind_textdomain_codeset(GETTEXT_PROGNAME, locale.nl_langinfo(locale.CODESET))
    gettext.bindtextdomain(GETTEXT_PROGNAME, '/usr/share/locale')
    gettext.textdomain(GETTEXT_PROGNAME)


old_stdout = -1
def mute_stdout():
    if verbose < 2:
        global old_stdout
        old_stdout = sys.stdout
        sys.stdout = open("/dev/null", "w")

def unmute_stdout():
    if verbose < 2:
        if old_stdout != -1:
            sys.stdout = old_stdout
        else:
            print "ERR: unmute called without mute?"

verbose = 0
def log1(message):
    """ prints log message if verbosity > 0 """
    if verbose > 0:
        print "LOG1:", message

def log2(message):
    """ prints log message if verbosity > 1 """
    if verbose > 1:
        print "LOG2:", message

#eu_unstrip_OUT=`eu-unstrip "--core=$core" -n 2>eu_unstrip.ERR`
def extract_info_from_core(coredump_name):
    """
    Extracts builds with filenames,
    Returns a list of tuples (build_id, filename)
    """
    #OFFSET = 0
    BUILD_ID = 1
    LIBRARY = 2
    #SEP = 3
    EXECUTABLE = 4

    print _("Analyzing coredump '%s'") % coredump_name
    eu_unstrip_OUT = Popen(["eu-unstrip","--core=%s" % coredump_name, "-n"], stdout=PIPE, bufsize=-1).communicate()[0]
    # parse eu_unstrip_OUT and return the list of build_ids

    # eu_unstrip_OUT = ("0x7f42362ca000+0x204000 c4d35d993598a6242f7525d024b5ec3becf5b447@0x7f42362ca1a0 /usr/lib64/libcanberra-gtk.so.0 - libcanberra-gtk.so.0\n"
                     # "0x3afa400000+0x210000 607308f916c13c3ad9ee503008d31fa671ba73ce@0x3afa4001a0 /usr/lib64/libcanberra.so.0 - libcanberra.so.0\n"
                     # "0x3afa400000+0x210000 607308f916c13c3ad9ee503008d31fa671ba73ce@0x3afa4001a0 /usr/lib64/libcanberra.so.0 - libcanberra.so.0\n"
                     # "0x3bc7000000+0x208000 3be016bb723e85779a23e111a8ab1a520b209422@0x3bc70001a0 /usr/lib64/libvorbisfile.so.3 - libvorbisfile.so.3\n"
                     # "0x7f423609e000+0x22c000 87f9c7d9844f364c73aa2566d6cfc9c5fa36d35d@0x7f423609e1a0 /usr/lib64/libvorbis.so.0 - libvorbis.so.0\n"
                     # "0x7f4235e99000+0x205000 b5bc98c125a11b571cf4f2746268a6d3cfa95b68@0x7f4235e991a0 /usr/lib64/libogg.so.0 - libogg.so.0\n"
                     # "0x7f4235c8b000+0x20e000 f1ff6c8ee30dba27e90ef0c5b013df2833da2889@0x7f4235c8b1a0 /usr/lib64/libtdb.so.1 - libtdb.so.1\n"
                     # "0x3bc3000000+0x209000 8ef56f789fd914e8d0678eb0cdfda1bfebb00b40@0x3bc30001a0 /usr/lib64/libltdl.so.7 - libltdl.so.7\n"
                     # "0x7f4231b64000+0x22b000 3ca5b83798349f78b362b1ea51c8a4bc8114b8b1@0x7f4231b641a0 /usr/lib64/gio/modules/libgvfsdbus.so - libgvfsdbus.so\n"
                     # "0x7f423192a000+0x218000 ad024a01ad132737a8cfc7c95beb7c77733a652d@0x7f423192a1a0 /usr/lib64/libgvfscommon.so.0 - libgvfscommon.so.0\n"
                     # "0x7f423192a000+0x218000 ad024a01ad132737a8cfc7c95beb7c77733a652d@0x7f423192a1a0 /usr/lib64/libgvfscommon.so.0 - libgvfscommon.so.0\n"
                     # "0x3bb8e00000+0x20e000 d240ac5755184a95c783bb98a2d05530e0cf958a@0x3bb8e001a0 /lib64/libudev.so.0 - libudev.so.0")
    #
    #print eu_unstrip_OUT
    # we failed to get build ids from the core -> die
    if not eu_unstrip_OUT:
        print "Can't get build ids from %s" % coredump_name
        return RETURN_FAILURE

    lines = eu_unstrip_OUT.split('\n')
    # using set ensures the unique values
    build_ids = set()
    libraries = set()
    build_ids = set()

    for line in lines:
        b_ids_line = line.split()
        if len(b_ids_line) > 2:
            # [exe] -> the executable itself
            # linux-vdso.so.1 -> Virtual Dynamic Shared Object
            if b_ids_line[EXECUTABLE] not in ["linux-vdso.so.1"]:
                build_id = b_ids_line[BUILD_ID].split('@')[0]
                build_ids.add(build_id)
                library = b_ids_line[LIBRARY]
                libraries.add(library)
                build_ids.add(build_id)
            else:
                log2("skipping line '%s'" % line)
    log1("Found %i build_ids" % len(build_ids))
    log1("Found %i libs" % len(libraries))
    return build_ids

def build_ids_to_path(build_ids):
    """
    build_id1=${build_id:0:2}
    build_id2=${build_id:2}
    file="usr/lib/debug/.build-id/$build_id1/$build_id2.debug"
    """
    return ["/usr/lib/debug/.build-id/%s/%s.debug" % (b_id[:2], b_id[2:]) for b_id in build_ids]

def sigterm_handler(signum, frame):
    exit(RETURN_OK)

def sigint_handler(signum, frame):
    print "\n", _("Exiting on user command")
    exit(RETURN_OK)

import signal

if __name__ == "__main__":
    # abrt-server can send SIGTERM to abort the download
    signal.signal(signal.SIGTERM, sigterm_handler)
    # ctrl-c
    signal.signal(signal.SIGINT, sigint_handler)
    core = None
    cachedir = None
    tmpdir = None
    keeprpms = False
    output_file = "build_ids"

    # localization
    init_gettext()

    ABRT_VERBOSE = os.getenv("ABRT_VERBOSE")
    if (ABRT_VERBOSE):
        try:
            verbose = int(ABRT_VERBOSE)
        except:
            pass

    help_text = _("Usage: %s --core=COREFILE ") % sys.argv[0]
    try:
        opts, args = getopt.getopt(sys.argv[1:], "vhc:o:", ["help", "core="])
    except getopt.GetoptError, err:
        print str(err) # prints something like "option -a not recognized"
        sys.exit(RETURN_FAILURE)

    for opt, arg in opts:
        if opt == "-v":
            verbose += 1
        elif opt == "-o":
            output_file = arg
        elif opt in ("--core", "-c"):
            core = arg
        elif opt in ("-h", "--help"):
            print help_text
            sys.exit()

    if not core:
        print _("You have to specify the path to coredump.")
        print help_text
        exit(RETURN_FAILURE)

    b_ids = extract_info_from_core(core)
    if b_ids == RETURN_FAILURE:
        exit(RETURN_FAILURE)
    f_build_ids = open(output_file, "w")
    for bid in b_ids:
        f_build_ids.write("%s\n" % bid)
    f_build_ids.close()
