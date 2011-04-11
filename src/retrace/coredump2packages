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

def binary_packages_from_debuginfo_package(debuginfo_package, binobj_path):
    """
    Returns a list of packages corresponding to the provided debuginfo
    package. One of the packages in the list contains the binary
    specified in binobj_path; this is a list because if binobj_patch
    is not specified (and sometimes it is not, binobj_path might
    contain just '-'), we do not know which package contains the
    binary, we know only packages from the same SRPM as the debuginfo
    package.
    """
    package_list = []
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
            package_list.append(package)
    else:
        log.write("   Yum search for {0}...\n".format(binobj_path))
        binobj_package_list = yumbase.pkgSack.searchFiles(binobj_path)
        for binobj_package in binobj_package_list:
            log.write("    - {0}".format(str(binobj_package)))
            if 0 != binobj_package.returnEVR().compare(debuginfo_package.returnEVR()):
                log.write(": NVR doesn't match\n")
                continue
            log.write(": NVR matches\n")
            package_list.append(binobj_package)
    return package_list

def process_unstrip_entry(build_id, binobj_path):
    """
    Returns a tuple of two items.

    First item is a list of packages which we found to be associated
    with the unstrip entry defined by build_id and binobj_path.

    Second item is a list of package versions (same package name,
    different epoch-version-release), which contain the binary object
    (an executable or shared library) corresponding to this unstrip
    entry. If this method failed to find an unique package name (with
    only different versions), this list contains the list of base
    package names. This item can be used to associate a coredump with
    some crashing package.
    """
    package_list = []
    coredump_package_list = []
    coredump_base_package_list = []
    # Ask for a known path from debuginfo package.
    debuginfo_path = "/usr/lib/debug/.build-id/{0}/{1}.debug".format(build_id[:2], build_id[2:])
    log.write("Yum search for {0}...\n".format(debuginfo_path))
    debuginfo_package_list = yumbase.pkgSack.searchFiles(debuginfo_path)

    # A problem here is that some libraries lack debuginfo. Either
    # they were stripped during build, or they were not stripped by
    # /usr/lib/rpm/find-debuginfo.sh because of wrong permissions or
    # something. The proper solution is to detect such libraries and
    # fix the packages.
    for debuginfo_package in debuginfo_package_list:
        log.write(" - {0}\n".format(str(debuginfo_package)))
        package_list.append(debuginfo_package)
        binary_packages = binary_packages_from_debuginfo_package(debuginfo_package, binobj_path)
        coredump_base_package_list.append(debuginfo_package.base_package_name)
        if len(binary_packages) == 1:
            coredump_package_list.append(str(binary_packages[0]))
        package_list.extend(binary_packages)
    if len(coredump_package_list) == len(coredump_base_package_list):
        return package_list, coredump_package_list
    else:
        return package_list, coredump_base_package_list


def process_unstrip_output():
    """
    Parse the eu-unstrip output, and search for packages via yum.

    Returns a tuple containing three items:
      - a list of package objects
      - a list of missing buildid entries
      - a list of coredump package adepts
    """
    # List of packages found in yum repositories and matching the
    # coredump.
    package_list = []
    # List of pairs (library/executable path, build id) which were not
    # found via yum.
    missing_buildid_list = []
    # coredump package adepts
    coredump_package_list = []
    first_entry = True
    for line in unstrip.split('\n'):
        parts = line.split()
        if not parts or len(parts) < 3:
            continue
        build_id = parts[1].split('@')[0]
        binobj_path = parts[2]
        if binobj_path[0] != '/' and parts[4] != '[exe]':
            continue
        entry_package_list, entry_coredump_package_list = process_unstrip_entry(build_id, binobj_path)
        if first_entry:
            coredump_package_list = entry_coredump_package_list
            first_entry = False
        if len(entry_package_list) == 0:
            missing_buildid_list.append([binobj_path, build_id])
        else:
            for entry_package in entry_package_list:
                found = False
                for package in package_list:
                    if str(entry_package) == str(package):
                        found = True
                        break
                if not found:
                    package_list.append(entry_package)
    return package_list, missing_buildid_list, coredump_package_list

package_list, missing_buildid_list, coredump_package_list = process_unstrip_output()

#
# The package list might contain multiple packages with the same name,
# but different version. This happens because some binary had the same
# build id over multiple package releases.
#
def find_duplicates(package_list):
    for p1 in range(0, len(package_list) - 1):
        package1 = package_list[p1]
        for p2 in range(p1 + 1, len(package_list)):
            package2 = package_list[p2]
            if package1.name == package2.name:
                return package1, package2
    return None, None

def count_removals(package_list, base_package_name, epoch, ver, rel, arch):
    count = 0
    for package in package_list:
        if package.base_package_name != base_package_name:
            continue
        if package.epoch != epoch or package.ver != ver or package.rel != rel or package.arch != arch:
            continue
        count += 1
    return count

log.write("Checking for duplicates...\n")
while True:
    package1, package2 = find_duplicates(package_list)
    if package1 is None:
        break
    p1removals = count_removals(package_list,
                                package1.base_package_name,
                                package1.epoch,
                                package1.ver,
                                package1.rel,
                                package1.arch)
    p2removals = count_removals(package_list,
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
    # Remove the removal_candidate packages from the package list
    for package in package_list[:]:
        if package.base_package_name == removal_candidate.base_package_name and \
                0 == package.returnEVR().compare(removal_candidate.returnEVR()):
            package_list.remove(package)

# Clean coredump_package_list:
for coredump_package in coredump_package_list[:]:
    found = False
    for package in package_list:
        if str(package) == coredump_package or package.base_package_name == coredump_package:
            found = True
            break
    if not found:
        coredump_package_list.remove(coredump_package)

#
# Print names of found packages first, then a newline separator, and
# then objects for which the packages were not found.
#
if len(coredump_package_list) == 1:
    print coredump_package_list[0]
else:
    print "-"
print
for package in sorted(package_list):
    print str(package)
print
for path, build_id in missing_buildid_list:
    print "{0} {1}".format(path, build_id)
