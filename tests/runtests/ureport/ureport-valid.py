#!/usr/bin/python
#
# Stolen from faf, namely from the faf/pyfaf/ureport.py file.
#
import re
import sys

# 2.0.12 | 2.0.13.35.g1033 | 2.0.12.26.gc7ab.dirty
ABRT_VERSION_PARSER = re.compile("^([0-9]+)\.([0-9]+)\.([0-9]+)(\..*)?$")

RE_ARCH = re.compile("^[0-9a-zA-Z_]+$")
RE_EXEC = re.compile("^[0-9a-zA-Z/_\.\-\+]+$")
RE_FUNCHASH = re.compile("^[a-zA-Z0-9\;\_\:\,\?]+$")
RE_HEX = re.compile("^(0[xX])?[0-9a-fA-F]+$")
RE_NONEMPTY = re.compile("^.+$")
RE_PACKAGE = re.compile("^[0-9a-zA-Z_\.\+\-~]+$")
RE_PHRASE = re.compile("^[0-9a-zA-Z_<>:\*\+=~@\?\!\ &(),\/\|\`\'\^\-\.\[\]\$\#]+$")
RE_PROJNAME = re.compile("^[0-9a-zA-Z \+\-\)\(\._~]+$")
RE_SEPOL = re.compile("^[a-zA-Z0-9_\.\-]+(:[a-zA-Z0-9_\.\-]+){3,4}$")
RE_TAINT = re.compile("^[A-Z ]+$")

MAX_UREPORT_LENGTH = 1 << 22 # 4MB
MAX_ATTACHMENT_LENGTH = 1 << 20 # 1MB (just metadata)

PACKAGE_CHECKER = {
  "name":         { "mand": True, "type": basestring, "re": RE_PACKAGE, "maxlen": 255 },
  "version":      { "mand": True, "type": basestring, "re": RE_PACKAGE, "maxlen": 255 },
  "release":      { "mand": True, "type": basestring, "re": RE_PACKAGE, "maxlen": 255 },
  "architecture": { "mand": True, "type": basestring, "re": RE_ARCH, "maxlen": 255 },
  "epoch":        { "mand": True, "type": int }
}

RELATED_PACKAGES_ELEM_CHECKER = {
  "installed_package": { "mand": True,  "type": dict, "checker": PACKAGE_CHECKER },
  "running_package":   { "mand": False, "type": dict, "checker": PACKAGE_CHECKER }
}

RELATED_PACKAGES_CHECKER = { "type": dict, "checker": RELATED_PACKAGES_ELEM_CHECKER }

NV_CHECKER = {
  "name":    { "mand": True, "type": basestring, "re": RE_PROJNAME, "maxlen": 255 },
  "version": { "mand": True, "type": basestring, "re": RE_PACKAGE, "maxlen": 255 }
}

SELINUX_CHECKER = {
  "mode":           { "mand": True,  "type": basestring , "re": re.compile("^(enforcing|permissive|disabled)$", re.IGNORECASE) },
  "context":        { "mand": False, "type": basestring,  "re": RE_SEPOL, "maxlen": 255 },
  "policy_package": { "mand": False, "type": dict, "checker": PACKAGE_CHECKER }
}

COREBT_ELEM_CHECKER = {
  "thread":   { "mand": True, "type": int },
  "frame":    { "mand": True, "type": int },
  "buildid":  { "mand": False, "type": basestring, "re": RE_PACKAGE, "maxlen": 255 },
  "path":     { "mand": False, "type": basestring, "re": RE_EXEC, "maxlen": 255 },
  "offset":   { "mand": True, "type": int },
  "funcname": { "mand": False, "type": basestring, "re": RE_PHRASE, "trunc": 255 },
  "funchash": { "mand": False, "type": basestring, "re": RE_FUNCHASH, "maxlen": 255 }
}

COREBT_CHECKER = { "type": dict, "checker": COREBT_ELEM_CHECKER }

PROC_STATUS_CHECKER = {

}

PROC_LIMITS_CHECKER = {

}

OS_STATE_CHECKER = {
    "suspend":  { "mand": True, "type": basestring, "re": re.compile("^(yes|no)$", re.IGNORECASE) },
    "boot":     { "mand": True, "type": basestring, "re": re.compile("^(yes|no)$", re.IGNORECASE) },
    "login":    { "mand": True, "type": basestring, "re": re.compile("^(yes|no)$", re.IGNORECASE) },
    "logout":   { "mand": True, "type": basestring, "re": re.compile("^(yes|no)$", re.IGNORECASE) },
    "shutdown": { "mand": True, "type": basestring, "re": re.compile("^(yes|no)$", re.IGNORECASE) }
}

UREPORT_CHECKER = {
  "ureport_version":   { "mand": False, "type": int },
  "type":              { "mand": True,  "type": basestring,  "re": re.compile("^(python|userspace|kerneloops)$", re.IGNORECASE) },
  "reason":            { "mand": True,  "type": basestring,  "re": RE_NONEMPTY, "trunc": 512 },
  "uptime":            { "mand": False, "type": int },
  "component":         { "mand": False, "type": basestring,  "re": RE_PACKAGE, "maxlen": 255 },
  "executable":        { "mand": False, "type": basestring,  "re": RE_EXEC, "maxlen": 255 },
  "installed_package": { "mand": True,  "type": dict, "checker": PACKAGE_CHECKER },
  "running_package":   { "mand": False, "type": dict, "checker": PACKAGE_CHECKER },
  "related_packages":  { "mand": True,  "type": list, "checker": RELATED_PACKAGES_CHECKER },
  "os":                { "mand": True,  "type": dict, "checker": NV_CHECKER },
  "architecture":      { "mand": True,  "type": basestring,  "re": RE_ARCH, "maxlen": 255 },
  "reporter":          { "mand": True,  "type": dict, "checker": NV_CHECKER },
  "crash_thread":      { "mand": True,  "type": int },
  "core_backtrace":    { "mand": True,  "type": list, "checker": COREBT_CHECKER },
  "user_type":         { "mand": False, "type": basestring,  "re": re.compile("^(root|nologin|local|remote)$", re.IGNORECASE) },
  "os_state":          { "mand": False, "type": dict,  "checker": OS_STATE_CHECKER },
  "selinux":           { "mand": False, "type": dict, "checker": SELINUX_CHECKER },
  "kernel_taint_state":{ "mand": False, "type": basestring,  "re": RE_TAINT, "maxlen": 255 },
  "proc_status":       { "mand": False, "type": dict, "checker": PROC_STATUS_CHECKER },
  "proc_limits":       { "mand": False, "type": dict, "checker": PROC_LIMITS_CHECKER },
  "oops":              { "mand": False, "type": basestring, "maxlen": 1 << 16 },
}

# just metadata, large objects are uploaded separately
ATTACHMENT_CHECKER = {
  "type":   { "mand": True, "type": basestring, "re": RE_PHRASE, "maxlen": 64 },
  "bthash": { "mand": True, "type": basestring, "re": RE_HEX,    "maxlen": 64 },
  "data":   { "mand": True, "type": basestring, "re": RE_PHRASE, "maxlen": 1024 },
}

def validate(obj, checker=UREPORT_CHECKER):
    expected = dict
    if "type" in checker and isinstance(checker["type"], type):
        expected = checker["type"]

    # check for expected type
    if not isinstance(obj, expected):
        raise Exception, "typecheck failed: expected {0}, had {1}; {2}".format(expected.__name__, type(obj).__name__, obj)

    # str checks
    if isinstance(obj, basestring):
        if "re" in checker and checker["re"].match(obj) is None:
            raise Exception, 'string "{0}" contains illegal characters'.format(obj)
        if "trunc" in checker and len(obj) > checker["trunc"]:
            obj = obj[:checker["trunc"]]
        if "maxlen" in checker and len(obj) > checker["maxlen"]:
            raise Exception, 'string "{0}" is too long (maximum {1})'.format(obj, checker["maxlen"])
    # list - apply checker["checker"] to every element
    elif isinstance(obj, list):
        obj = [validate(elem, checker["checker"]) for elem in obj]

    # dict
    elif isinstance(obj, dict):
        # load the actual checker if we are not toplevel
        if "checker" in checker:
            checker = checker["checker"]

        # need to clone, we are going to modify
        clone = dict(obj)
        obj = dict()
        # validate each element separately
        for key in checker:
            subchkr = checker[key]
            try:
                value = clone.pop(key)
            except KeyError:
                # fail for mandatory elements
                if subchkr["mand"]:
                    raise Exception, "missing mandatory element '{0}'".format(key)
                # just skip optional
                continue

            try:
                obj[key] = validate(value, subchkr)
            except Exception, msg:
                # queue error messages
                raise Exception, "error validating '{0}': {1}".format(key, msg)

        # excessive elements - error
        keys = clone.keys()
        if keys:
            raise Exception, "unknown elements present: {0}".format(keys)

    return obj

if __name__ == "__main__":
    try:
        with open(sys.argv[1], 'r') as fh:
            import json
            ureport = json.load(fh)

        ureport = validate(ureport)
        print "THANKYOU"
    except Exception as ex:
        sys.exit("ERROR {0}".format(str(ex)))
