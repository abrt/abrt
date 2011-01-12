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
args = parser.parse_args()

#
# Initialize yum, enable only repositories specified via command line
# --repos option.
#
stdout = sys.stdout
sys.stdout = open("/dev/null", "w")
yumbase = yum.YumBase()
yumbase.doConfigSetup()
if not yumbase.setCacheDir():
    exit(2)
for repo in yumbase.repos.listEnabled():
    repo.close()
    yumbase.repos.disableRepo(repo.id)
for repo in yumbase.repos.findRepos(args.repos):
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
unstrip_args = ['eu-unstrip', '--core={0}'.format(args.coredump), '-n']
unstrip_proc = subprocess.Popen(unstrip_args, stdout=subprocess.PIPE)
unstrip = unstrip_proc.communicate()[0]
if not unstrip:
    exit(1)

#
# Parse the eu-unstrip output, and search for packages via yum.
#
# List of packages found in yum repositories and matching the
# coredump.
package_list = []
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
    debuginfo_package_list = yumbase.pkgSack.searchFiles(debuginfo_path)
    if not debuginfo_package_list:
        missing_debuginfo_list.append([binobj_path, build_id])
        continue
    if not debuginfo_package_list[0] in package_list:
        # New debuginfo package was found.  Store it and search for
        # corresponding package with the library/executable binary
        # itself.
        package_list.append(debuginfo_package_list[0])
        binobj_package_list = yumbase.pkgSack.searchFiles(binobj_path)
        if not binobj_package_list:
            missing_package_list.append([binobj_path, build_id])
            continue
        package_found = False
        for binobj_package in binobj_package_list:
            if 0 != binobj_package.returnEVR().compare(debuginfo_package_list[0].returnEVR()):
                continue
            if not binobj_package in package_list:
                package_list.append(binobj_package)
            package_found = True
            break
        if not package_found:
            missing_package_list.append([binobj_path, build_id])

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
