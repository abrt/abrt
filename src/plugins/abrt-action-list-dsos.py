#! /usr/bin/python -u
import rpm

def list_dsos_rpm(dso_paths):

    ts = rpm.TransactionSet()
    for path in dso_paths:
        mi = ts.dbMatch('basenames', path)
        if len(mi):
            for h in mi:
                print  "%s <> %s - (%s)" % (path, h[rpm.RPMTAG_NEVRA], h[rpm.RPMTAG_VENDOR])
        else:
            print "%s doesn't belong to any package" % (path)


def parse_maps(maps_path):
    f = open(maps_path, "r")
    return [x.strip()[x.find('/'):] for x in f.readlines() if x.find('/') > -1]


if __name__ == "__main__":
    try:
        dsos = parse_maps("maps")
        list_dsos_rpm(dsos)
    except Exception, ex:
        print "Couldn't get the dsos list: %s", ex
