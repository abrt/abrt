#!/usr/bin/python
#
# Stolen from faf, namely from the faf/pyfaf/ureport.py file.
#
import re
import sys

# 2.0.12 | 2.0.13.35.g1033 | 2.0.12.26.gc7ab.dirty
ABRT_VERSION_PARSER = re.compile("^([0-9]+)\.([0-9]+)\.([0-9]+)(\..*)?$")

RE_ARCH = re.compile("^[0-9a-zA-Z_]+$")
RE_EXEC = re.compile("^[0-9a-zA-Z<>/_\.\-\+]+$")
RE_FUNCHASH = re.compile("^[a-zA-Z0-9\;\_\:\,\?]+$")
RE_HEX = re.compile("^(0[xX])?[0-9a-fA-F]+$")
RE_NONEMPTY = re.compile("^.+$")
RE_PACKAGE = re.compile("^[0-9a-zA-Z_\.\+\-~]+$")
RE_PHRASE = re.compile("^[0-9a-zA-Z_<>:\*\+=~@\?\!\ &(),\/\|\`\'\^\-\.\[\]\$\#]+$")
RE_PROJNAME = re.compile("^[0-9a-zA-Z \+\-\)\(\._~]+$")
# 17, 12.2, 6.4, 7.0 Alpha3, 6.4 Beta, Rawhide, Tumbleweed
RE_OSVERSION = re.compile("^[0-9a-zA-Z]+(\.[0-9]+)?( (Alpha|Beta)[0-9]*)?$")
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

OS_CHECKER = {
  "name":    { "mand": True, "type": basestring, "re": RE_PROJNAME, "maxlen": 255 },
  "version": { "mand": True, "type": basestring, "re": RE_OSVERSION, "maxlen": 255 }
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
  "serial":            { "mand": True,  "type": int },
  "component":         { "mand": False, "type": basestring,  "re": RE_PACKAGE, "maxlen": 255 },
  "executable":        { "mand": False, "type": basestring,  "re": RE_EXEC, "maxlen": 255 },
  "installed_package": { "mand": True,  "type": dict, "checker": PACKAGE_CHECKER },
  "running_package":   { "mand": False, "type": dict, "checker": PACKAGE_CHECKER },
  "related_packages":  { "mand": True,  "type": list, "checker": RELATED_PACKAGES_CHECKER },
  "os":                { "mand": True,  "type": dict, "checker": OS_CHECKER },
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

def ureport2to1(ureport2):
    if "ureport_version" not in ureport2 or ureport2["ureport_version"] != 2:
        raise ValueError, "uReport2 is required"

    ureport1 = { "ureport_version": 1, }

    if "reporter" in ureport2:
        reporter = ureport2["reporter"]

        ureport1["reporter"] = {}
        if "name" in reporter:
            ureport1["reporter"]["name"] = reporter["name"]

        if "version" in reporter:
            ureport1["reporter"]["version"] = reporter["version"]

    if "os" in ureport2:
        opsys = ureport2["os"]

        ureport1["os"] = {}
        if "name" in opsys:
            ureport1["os"]["name"] = opsys["name"].capitalize()

        if "version" in opsys:
            ureport1["os"]["version"] = opsys["version"]

        if "architecture" in opsys:
            ureport1["architecture"] = opsys["architecture"]

    if "packages" in ureport2:
        ureport1["related_packages"] = []

        for package in ureport2["packages"]:
            pkg1 = {}
            if "name" in package:
                 pkg1["name"] = package["name"]

            if "epoch" in package:
                 pkg1["epoch"] = package["epoch"]

            if "version" in package:
                 pkg1["version"] = package["version"]

            if "release" in package:
                 pkg1["release"] = package["release"]

            if "architecture" in package:
                 pkg1["architecture"] = package["architecture"]

            if ("package_role" in package and
                package["package_role"] == "affected"):
                 ureport1["installed_package"] = pkg1
            else:
                 ureport1["related_packages"].append({"installed_package": pkg1})

    if "problem" in ureport2:
        prob = ureport2["problem"]
        if "type" in prob:
            if prob["type"].lower() == "core":
                ureport1["type"] = "userspace"
            else:
                ureport1["type"] = prob["type"]

        if "component" in prob:
            ureport1["component"] = prob["component"]

        if "executable" in prob:
            ureport1["executable"] = prob["executable"]

        if "serial" in prob:
            ureport1["serial"] = prob["serial"]

        if "user" in prob:
            user = prob["user"]

            if "root" in user and user["root"]:
                ureport1["user_type"] = "root"
            elif "local" in user and not user["local"]:
                ureport1["user_type"] = "remote"
            else:
                ureport1["user_type"] = "local"

        if "stacktrace" in prob or "frames" in prob:
            ureport1["core_backtrace"] = []

            if ureport1["type"] == "userspace":
                threads = prob["stacktrace"]
            elif ureport1["type"] == "kerneloops":
                threads = [{ "crash_thread": True,
                             "frames": prob["frames"], }]
            else:
                threads = [{ "crash_thread": True,
                             "frames": prob["stacktrace"], }]

            tid = 0
            for thread in threads:
                tid += 1

                if "crash_thread" in thread and thread["crash_thread"]:
                    ureport1["crash_thread"] = tid

                fid = 0
                for frame in thread["frames"]:
                    fid += 1

                    new_frame = { "thread": tid, "frame": fid, }

                    if ureport1["type"] == "userspace":
                        if "build_id" in frame:
                            new_frame["buildid"] = frame["build_id"]

                        if "build_id_offset" in frame:
                            new_frame["offset"] = frame["build_id_offset"]

                        if "file_name" in frame:
                            new_frame["path"] = frame["file_name"]

                        if "fingerprint" in frame:
                            new_frame["funchash"] = frame["fingerprint"]

                        if "function_name" in frame:
                            new_frame["funcname"] = frame["function_name"]
                        else:
                            new_frame["funcname"] = "??"
                    elif ureport1["type"] == "kerneloops":
                        new_frame["buildid"] = ("{0}-{1}.{2}"
                            .format(ureport1["installed_package"]["version"],
                                    ureport1["installed_package"]["release"],
                                    ureport1["installed_package"]["architecture"]))

                        if ureport1["installed_package"]["name"].startswith("kernel-"):
                            new_frame["buildid"] += (".{0}"
                                .format(ureport1["installed_package"]["name"][7:]))

                        if "function_name" in frame:
                            new_frame["funcname"] = frame["function_name"]

                        if "reliable" in frame and not frame["reliable"]:
                            new_frame["funchash"] = "?"

                        if "function_offset" in frame:
                            new_frame["offset"] = frame["function_offset"]

                        if "module_name" in frame:
                            new_frame["path"] = frame["module_name"]
                        else:
                            new_frame["path"] = "vmlinux"
                    elif ureport1["type"] == "python":
                        if "file_name" in frame:
                            new_frame["path"] = frame["file_name"]

                        if "special_file" in frame:
                            new_frame["path"] = ("<{0}>"
                                .format(frame["special_file"]))

                        if "file_line" in frame:
                            new_frame["offset"] = frame["file_line"]

                        if "function_name" in frame:
                            new_frame["funcname"] = frame["function_name"]

                        if "special_function" in frame:
                            new_frame["funcname"] = ("<{0}>"
                                .format(frame["special_function"]))
                    else:
                        raise ValueError, ("type '{0}' is not supported"
                                           .format(ureport1["type"]))

                    ureport1["core_backtrace"].append(new_frame)

    if "reason" in ureport2:
        ureport1["reason"] = ureport2["reason"]

    return ureport1

def validate(obj, checker=UREPORT_CHECKER):
    if (checker == UREPORT_CHECKER and
        "ureport_version" in obj and
        obj["ureport_version"] == 2):
        obj = ureport2to1(obj)

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
        sys.exit(70)
    except Exception as ex:
        sys.exit("ERROR {0}".format(str(ex)))
