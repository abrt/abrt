#! /usr/bin/python
# -*- coding:utf-8;mode:python -*-
# Gets list of packages necessary for processing of a coredump.
# Uses eu-unstrip and yum.

import subprocess
import yum
import sys
import argparse

parser = argparse.ArgumentParser(description='Get packages for coredump processing.')
parser.add_argument('--repos', default='*', metavar='WILDCARD',
                    help='Yum repository wildcard to be enabled')
parser.add_argument('coredump', help='Coredump')
parser.add_argument('--log', metavar='FILENAME',
                    help='Store debug output to a file')
args = parser.parse_args()

if args.log:
    log = open(args.log, "w")
else:
    log = open("/dev/null", "w")

#
# Initialize yum, enable only repositories specified via command line
# --repos option.
#
stdout = sys.stdout
sys.stdout = log
yumbase = yum.YumBase()
yumbase.doConfigSetup()
if not yumbase.setCacheDir():
    exit(2)
log.write("Closing all enabled repositories...\n")
for repo in yumbase.repos.listEnabled():
    log.write(" - {0}\n".format(repo.name))
    repo.close()
    yumbase.repos.disableRepo(repo.id)
log.write("Enabling repositories matching \'{0}\'...\n".format(args.repos))
for repo in yumbase.repos.findRepos(args.repos):
    log.write(" - {0}\n".format(repo.name))
    repo.enable()
    repo.skip_if_unavailable = True
yumbase.repos.doSetup()
yumbase.repos.populateSack(mdtype='metadata', cacheonly=1)
yumbase.repos.populateSack(mdtype='filelists', cacheonly=1)
sys.stdout = stdout

#
# Get eu-unstrip output, which contains build-ids and binary object
# paths
#
log.write("Running eu-unstrip...\n")
unstrip_args = ['eu-unstrip', '--core={0}'.format(args.coredump), '-n']
unstrip_proc = subprocess.Popen(unstrip_args, stdout=subprocess.PIPE)
unstrip = unstrip_proc.communicate()[0]
log.write("{0}\n".format(unstrip))
if not unstrip:
    exit(1)

#
# Parse the eu-unstrip output, and search for packages via yum.
#
# List of packages found in yum repositories and matching the
# coredump.
package_list = []
package_to_object = {}
# List of pairs (library/executable path, build id) which were not
# found via yum.
missing_debuginfo_list = []
# List of pairs (library/executable path, build id) which were not
# found via yum, but the debuginfo package for them was found.  If
# this happens, the repositories or their medatada are wrong.
missing_package_list = []
for line in unstrip.split('\n'):
    parts = line.split()
    if not parts or len(parts) < 3:
        continue
    build_id = parts[1].split('@')[0]
    binobj_path = parts[2]
    if binobj_path[0] != '/' and parts[4] != '[exe]':
        continue
    # Ask for a known path from debuginfo package.
    debuginfo_path = "/usr/lib/debug/.build-id/{0}/{1}.debug".format(build_id[:2], build_id[2:])
    log.write("Yum search for {0}...\n".format(debuginfo_path))
    debuginfo_package_list = yumbase.pkgSack.searchFiles(debuginfo_path)
    if not debuginfo_package_list:
        missing_debuginfo_list.append([binobj_path, build_id])
        continue
    for debuginfo_package in debuginfo_package_list:
        log.write(" - {0}\n".format(str(debuginfo_package)))
        if str(debuginfo_package) in package_list:
            continue

        # New debuginfo package was found.  Store it and search for
        # corresponding package with the library/executable binary
        # itself.
        package_list.append(str(debuginfo_package))
        package_to_object[str(debuginfo_package)] = debuginfo_package


        if binobj_path == '-': # [exe] without binary name
            log.write("   Yum search for [exe] without binary name, "
                      "packages with NVR {0}:{1}-{2}.{3}...\n".format(debuginfo_package.epoch,
                                                                      debuginfo_package.ver,
                                                                      debuginfo_package.rel,
                                                                      debuginfo_package.arch))
            # Append all packages with the same base package name.
            # Other possibility is to download the debuginfo RPM,
            # unpack it, and get the name of the binary from the
            # /usr/lib/debug/.build-id/xx/yyyyyy symlink.
            evra_list = yumbase.pkgSack.searchNevra(epoch=debuginfo_package.epoch,
                                                    ver=debuginfo_package.ver,
                                                    rel=debuginfo_package.rel,
                                                    arch=debuginfo_package.arch)
            for package in evra_list:
                log.write("    - {0}: base name \"{1}\"\n".format(str(package), package.base_package_name))
                if package.base_package_name != debuginfo_package.base_package_name:
                    continue
                if not str(package) in package_list:
                    package_list.append(str(package))
                    package_to_object[str(package)] = package
            continue

        log.write("   Yum search for {0}...\n".format(binobj_path))
        binobj_package_list = yumbase.pkgSack.searchFiles(binobj_path)
        if not binobj_package_list:
            missing_package_list.append([binobj_path, build_id])
            continue
        package_found = False
        for binobj_package in binobj_package_list:
            log.write("    - {0}".format(str(binobj_package)))
            if 0 != binobj_package.returnEVR().compare(debuginfo_package.returnEVR()):
                log.write(": NVR doesn't match\n")
                continue
            log.write(": NVR matches\n")
            if not str(binobj_package) in package_list:
                package_list.append(str(binobj_package))
                package_to_object[str(binobj_package)] = binobj_package
            package_found = True
            break
        if not package_found:
            missing_package_list.append([binobj_path, build_id])

#
# The package list might contain multiple packages with the same name,
# but different version. This happens because some binary had the same
# build id over multiple package releases.
#
def find_duplicates(package_objects):
    for p1 in range(0, len(package_objects) - 1):
        package1 = package_objects[p1]
        for p2 in range(p1 + 1, len(package_objects)):
            package2 = package_objects[p2]
            if package1.name == package2.name:
                return package1, package2
    return None, None

def count_removals(package_objects, base_package_name, epoch, ver, rel, arch):
    count = 0
    for package in package_objects:
        if package.base_package_name != base_package_name:
            continue
        if package.epoch != epoch or package.ver != ver or package.rel != rel or package.arch != arch:
            continue
        count += 1
    return count

log.write("Checking for duplicates...\n")
while True:
    package1, package2 = find_duplicates(package_to_object.values())
    if package1 is None:
        break
    p1removals = count_removals(package_to_object.values(),
                                package1.base_package_name,
                                package1.epoch,
                                package1.ver,
                                package1.rel,
                                package1.arch)
    p2removals = count_removals(package_to_object.values(),
                                package2.base_package_name,
                                package2.epoch,
                                package2.ver,
                                package2.rel,
                                package2.arch)

    log.write(" - {0}".format(package1.base_package_name))
    if package1.base_package_name != package2.base_package_name:
        log.write(" {0}\n".format(package2.base_package_name))
    else:
        log.write("\n")
    log.write("   - {0}:{1}-{2}.{3} ({4} dependent packages)\n".format(package1.epoch,
                                                                       package1.ver,
                                                                       package1.rel,
                                                                       package1.arch,
                                                                       p1removals))
    log.write("   - {0}:{1}-{2}.{3} ({4} dependent packages)\n".format(package2.epoch,
                                                                       package2.ver,
                                                                       package2.rel,
                                                                       package2.arch,
                                                                       p2removals))

    removal_candidate = package1
    if p1removals == p2removals:
        # Remove older if we can choose
        if package1.returnEVR().compare(package2.returnEVR()) > 0:
            removal_candidate = package2
        log.write("   - decided to remove {0}:{1}-{2}.{3} because it's older\n".format(removal_candidate.epoch,
                                                                                       removal_candidate.ver,
                                                                                       removal_candidate.rel,
                                                                                       removal_candidate.arch))
    else:
        if p1removals > p2removals:
            removal_candidate = package2
        log.write("   - decided to remove {0}:{1}-{2}.{3} because has fewer dependencies\n".format(removal_candidate.epoch,
                                                                                                   removal_candidate.ver,
                                                                                                   removal_candidate.rel,
                                                                                                   removal_candidate.arch))

    # Remove the removal_candidate packages from the list
    for package in package_list[:]:
        if package_to_object[package].base_package_name == removal_candidate.base_package_name and \
                0 == package_to_object[package].returnEVR().compare(removal_candidate.returnEVR()):
            package_list.remove(package)
            del package_to_object[package]

#
# Print names of found packages first, then a newline separator, and
# then objects for which the packages were not found.
#
for package in sorted(package_list):
    print package
print
for path, build_id in missing_debuginfo_list:
    print "{0} {1}".format(path, build_id)
for path, build_id in missing_package_list:
    print "{0} {1}".format(path, build_id)
