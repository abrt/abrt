#!/usr/bin/python -u
# WARNING: python -u means unbuffered I/O. Without it the messages are
# passed to the parent asynchronously which looks bad in clients.

import sys
import os
import getopt
import rpm

def log(s):
    sys.stderr.write("%s\n" % s)

def error_msg(s):
    sys.stderr.write("%s\n" % s)

def error_msg_and_die(s):
    sys.stderr.write("%s\n" % s)
    sys.exit(1)

def xopen(name, mode):
    try:
        r = open(name, mode)
    except IOError, e:
        error_msg_and_die("Can't open '%s': %s" % (name, e))
    return r


def parse_maps(maps_path):
    try:
        f = xopen(maps_path, "r")
        # set() -> uniqifies the list of filenames
        return set([x.strip()[x.find('/'):] for x in f.readlines() if x.find('/') > -1])
    except IOError, e:
        error_msg_and_die("Can't read '%s': %s" % (maps_path, e))

if __name__ == "__main__":
    progname = os.path.basename(sys.argv[0])
    help_text = ("Usage: %s [-o OUTFILE] -m PROC_PID_MAP_FILE") % progname
    try:
        opts, args = getopt.getopt(sys.argv[1:], "o:m:h", ["help"])
    except getopt.GetoptError, err:
        error_msg(err) # prints something like "option -a not recognized"
        error_msg_and_die(help_text)

    opt_o = None
    memfile = None

    for opt, arg in opts:
        if opt in ("-h", "--help"):
            print help_text
            exit(0)
        #elif opt == "-v":
        #    verbose += 1
        elif opt == "-o":
            opt_o = arg
        elif opt == "-m":
            memfile = arg

    if not memfile:
        error_msg("MAP_FILE is not specified")
        error_msg_and_die(help_text)

    try:
        # Note that we open -o FILE only when we reach the point
        # when we are definitely going to write something to it
        outfile = sys.stdout
        outname = opt_o
        try:
            dso_paths = parse_maps(memfile)
            for path in dso_paths:
                ts = rpm.TransactionSet()
                mi = ts.dbMatch('basenames', path)
                if len(mi):
                    for h in mi:
                        if outname:
                            outfile = xopen(outname, "w")
                            outname = None
                        outfile.write("%s %s (%s) %s\n" %
                                    (path,
                                     h[rpm.RPMTAG_NEVRA],
                                     h[rpm.RPMTAG_VENDOR],
                                     h[rpm.RPMTAG_INSTALLTIME])
                                    )

        except Exception, ex:
            error_msg_and_die("Can't get the DSO list: %s" % ex)

        outfile.close()
    except:
        if not opt_o:
            opt_o = "<stdout>"
        error_msg_and_die("Error writing to '%s'" % opt_o)
