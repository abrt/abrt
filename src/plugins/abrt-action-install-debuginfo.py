#! /usr/bin/python -u
# -*- coding: utf-8 -*-

# WARNING: python -u means unbuffered I/O without it the messages are
# passed to the parent asynchronously which looks bad in clients..
from subprocess import Popen, PIPE
import sys
import os
import time
import getopt
import shutil
from yum import _, YumBase
from yum.callbacks import DownloadBaseCallback

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


def error_msg_and_die(s):
    sys.stderr.write("%s\n" % s)
    os.exit(1)


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

def ask_yes_no(prompt, retries=4):
    while True:
        try:
            response = raw_input(prompt)
        except EOFError:
            log1("got eof, probably executed from helper, assuming - yes")
            response = 'y'
        if response in ('y', 'Y'):
            return True
        if response in ('n', 'N', ''):
            return False
        retries = retries - 1
        if retries < 0:
            break
    return False

# TODO: unpack just required debuginfo and not entire rpm?
# ..that can lead to: foo.c No such file and directory
# files is not used...
def unpack_rpm(package_nevra, files, tmp_dir, destdir, keeprpm):
    package_name = package_nevra + ".rpm"
    package_full_path = tmp_dir + "/" + package_name
    log1("Extracting %s to %s" % (package_full_path, destdir))
    log2(files)
    print _("Extracting cpio from %s") % (package_full_path)
    unpacked_cpio_path = tmp_dir + "/unpacked.cpio"
    try:
        unpacked_cpio = open(unpacked_cpio_path, 'wb')
    except IOError, ex:
        print _("Can't write to '%s': %s") % (unpacked_cpio_path, ex)
        return RETURN_FAILURE
    rpm2cpio = Popen(["rpm2cpio", package_full_path],
                       stdout = unpacked_cpio, bufsize = -1)
    retcode = rpm2cpio.wait()

    if retcode == 0:
        log1("cpio written OK")
        if not keeprpm:
            log1("keeprpms = False, removing %s" % package_full_path)
            #print _("Removing temporary rpm file")
            os.unlink(package_full_path)
    else:
        unpacked_cpio.close()
        print _("Can't extract package '%s'") % package_full_path
        return RETURN_FAILURE

    # close the file
    unpacked_cpio.close()
    # and open it for reading
    unpacked_cpio = open(unpacked_cpio_path, 'rb')

    print _("Caching files from %s made from %s") % ("unpacked.cpio", package_name)
    cpio = Popen(["cpio","-i", "-d", "--quiet"],
                  stdin=unpacked_cpio, cwd=destdir, bufsize=-1)
    retcode = cpio.wait()

    if retcode == 0:
        log1("files extracted OK")
        #print _("Removing temporary cpio file")
        os.unlink(unpacked_cpio_path)
    else:
        print _("Can't extract files from '%s'") % unpacked_cpio_path
        return RETURN_FAILURE

class MyDownloadCallback(DownloadBaseCallback):
    def __init__(self, total_pkgs):
        self.total_pkgs = total_pkgs
        self.downloaded_pkgs = 0
        self.last_pct = 0
        self.last_time = 0
        DownloadBaseCallback.__init__(self)

    def updateProgress(self, name, frac, fread, ftime):
        pct = int(frac * 100)
        if pct == self.last_pct:
            log2("percentage is the same, not updating progress")
            return

        self.last_pct = pct
        # if run from terminal we can have fancy output
        if sys.stdout.isatty():
            sys.stdout.write("\033[sDownloading (%i of %i) %s: %3u%%\033[u"
                    % (self.downloaded_pkgs + 1, self.total_pkgs, name, pct)
            )
            if pct == 100:
                print (_("Downloading (%i of %i) %s: %3u%%")
                        % (self.downloaded_pkgs + 1, self.total_pkgs, name, pct)
                )
        # but we want machine friendly output when spawned from abrt-server
        else:
            t = time.time()
            if self.last_time == 0:
                self.last_time = t
            # update only every 10 seconds
            if pct == 100 or self.last_time > t or t - self.last_time >= 10:
                print (_("Downloading (%i of %i) %s: %3u%%")
                        % (self.downloaded_pkgs + 1, self.total_pkgs, name, pct)
                )
                self.last_time = t
                if pct == 100:
                    self.last_time = 0

        sys.stdout.flush()

class DebugInfoDownload(YumBase):
    def __init__(self, cache, tmp, keep_rpms=False):
        self.cachedir = cache
        self.tmpdir = tmp
        self.keeprpms = keep_rpms
        YumBase.__init__(self)
        mute_stdout()
        #self.conf.cache = os.geteuid() != 0
        # Setup yum (Ts, RPM db, Repo & Sack)
        try:
            # Saw this exception here:
            # cannot open Packages index using db3 - Permission denied (13)
            # yum.Errors.YumBaseError: Error: rpmdb open failed
            self.doConfigSetup()
        except Exception, e:
            unmute_stdout()
            print _("Error initializing yum (YumBase.doConfigSetup): '%s'") % str(e)
            #return 1 - can't do this in constructor
            exit(1)
        unmute_stdout()

    # return value will be used as exitcode. So 0 = ok, !0 - error
    def download(self, files):
        """ @files - """
        installed_size = 0
        total_pkgs = 0
        todownload_size = 0
        downloaded_pkgs = 0
        # nothing to download?
        if not files:
            return

        #if verbose == 0:
        #    # this suppress yum messages about setting up repositories
        #    mute_stdout()

        # make yumdownloader work as non root user
        if not self.setCacheDir():
            self.logger.error("Error: can't make cachedir, exiting")
            exit(50)

        # disable all not needed
        for repo in self.repos.listEnabled():
            repo.close()
            self.repos.disableRepo(repo.id)
        # enable -debuginfo repos
        for repo in self.repos.findRepos(pattern="*debuginfo*"):
            #print repo
            repo.enable()
            rid = self.repos.enableRepo(repo.id)
            log1("enabled repo %s" % rid)
            setattr(repo, "skip_if_unavailable", True)

        self.repos.doSetup()

        # This is somewhat "magic", it unpacks the metadata making it usable.
        # Looks like this is the moment when yum talks to remote servers,
        # which takes time (sometimes minutes), let user know why
        # we have "paused":
        print _("Looking for needed packages in repositories")
        try:
            self.repos.populateSack(mdtype='metadata', cacheonly=1)
        except Exception, e:
            print _("Error retrieving metadata: '%s'") % str(e)
            return 1
        try:
            # Saw this exception here:
            # raise Errors.NoMoreMirrorsRepoError, errstr
            # NoMoreMirrorsRepoError: failure:
            # repodata/7e6632b82c91a2e88a66ad848e231f14c48259cbf3a1c3e992a77b1fc0e9d2f6-filelists.sqlite.bz2
            # from fedora-debuginfo: [Errno 256] No more mirrors to try.
            self.repos.populateSack(mdtype='filelists', cacheonly=1)
        except Exception, e:
            print _("Error retrieving filelists: '%s'") % str(e)
            return 1

        #if verbose == 0:
        #    # re-enable the output to stdout
        #    unmute_stdout()

        not_found = []
        package_files_dict = {}
        for debuginfo_path in files:
            log2("yum whatprovides %s" % debuginfo_path)
            pkg = self.pkgSack.searchFiles(debuginfo_path)
            # sometimes one file is provided by more rpms, we can use either of
            # them, so let's use the first match
            if pkg:
                if pkg[0] in package_files_dict.keys():
                    package_files_dict[pkg[0]].append(debuginfo_path)
                else:
                    package_files_dict[pkg[0]] = [debuginfo_path]
                    todownload_size += float(pkg[0].size)
                    installed_size += float(pkg[0].installedsize)
                    total_pkgs += 1

                log2("found pkg for %s: %s" % (debuginfo_path, pkg[0]))
            else:
                log2("not found pkg for %s" % debuginfo_path)
                not_found.append(debuginfo_path)

        # connect our progress update callback
        dnlcb = MyDownloadCallback(total_pkgs)
        self.repos.setProgressBar(dnlcb)

	if verbose != 0 or len(not_found) != 0:
	    print _("Can't find packages for %u debuginfo files") % len(not_found)
	if verbose != 0 or total_pkgs != 0:
	    print _("Packages to download: %u") % total_pkgs
	    print _("Downloading %.2fMb, installed size: %.2fMb") % (
	             todownload_size / (1024**2),
	             installed_size / (1024**2)
	            )

        # ask only if we have terminal, because for now we don't have a way
        # how to pass the question to gui and the response back
        if noninteractive == False and sys.stdout.isatty():
            if not ask_yes_no(_("Is this ok? [y/N] ")):
                return RETURN_OK

        for pkg, files in package_files_dict.iteritems():
            dnlcb.downloaded_pkgs = downloaded_pkgs
            repo.cache = 0
            remote = pkg.returnSimple('relativepath')
            local = os.path.basename(remote)
            if not os.path.exists(self.tmpdir):
                os.makedirs(self.tmpdir)
            if not os.path.exists(self.cachedir):
                os.makedirs(self.cachedir)
            local = os.path.join(self.tmpdir, local)
            pkg.localpath = local # Hack: to set the localpath we want
            ret = self.downloadPkgs(pkglist=[pkg])
            # downloadPkgs return an non empty list on succes
            if ret:
                print (_("Downloading package %s failed") % pkg)
            else:
                # normalize the name
                # just str(pkg) doesn't work because it can have epoch
                pkg_nvra = (pkg.name +"-"+ pkg.version +"-"+
                            pkg.release +"."+ pkg.arch)

                unpack_result = unpack_rpm(pkg_nvra, files, self.tmpdir,
                                           self.cachedir, keeprpms)
                if unpack_result == RETURN_FAILURE:
                    # recursively delete the temp dir on failure
                    print _("Unpacking failed, aborting download...")
                    clean_up()
                    return RETURN_FAILURE

            downloaded_pkgs += 1

        if not self.keeprpms and os.path.exists(self.tmpdir):
            print (_("All downloaded packages have been extracted, removing %s")
                 % self.tmpdir)
            try:
                os.rmdir(self.tmpdir)
            except OSError:
                print _("Can't remove %s, probably contains an error log") % self.tmpdir

verbose = 0
def log1(message):
    """ prints log message if verbosity > 0 """
    if verbose > 0:
        print "LOG1:", message

def log2(message):
    """ prints log message if verbosity > 1 """
    if verbose > 1:
        print "LOG2:", message

def build_ids_to_path(build_ids):
    """
    build_id1=${build_id:0:2}
    build_id2=${build_id:2}
    file="usr/lib/debug/.build-id/$build_id1/$build_id2.debug"
    """
    return ["/usr/lib/debug/.build-id/%s/%s.debug" % (b_id[:2], b_id[2:]) for b_id in build_ids]

# beware this finds only missing libraries, but not the executable itself ..

def filter_installed_debuginfos(build_ids, cache_dir):
    # 1st pass -> search in /usr/lib
    missing_di = []
    files = build_ids_to_path(build_ids)
    for debuginfo_path in files:
        cache_debuginfo_path = cache_dir + debuginfo_path
        log2("checking path: %s" % debuginfo_path)
        if os.path.exists(debuginfo_path):
            log2("found: %s" % debuginfo_path)
            continue
        if os.path.exists(cache_debuginfo_path):
            log2("found: %s" % cache_debuginfo_path)
            continue
        log2("not found: %s" % (cache_debuginfo_path))
        missing_di.append(debuginfo_path)
    return missing_di

tmpdir = None
def clean_up():
    try:
        shutil.rmtree(tmpdir)
    except OSError, ex:
        print _("Can't remove '%s': %s") % (tmpdir, ex)

def sigterm_handler(signum, frame):
    clean_up()
    exit(RETURN_OK)

def sigint_handler(signum, frame):
    clean_up()
    print "\n", _("Exiting on user command")
    exit(RETURN_OK)

import signal

if __name__ == "__main__":
    # abrt-server can send SIGTERM to abort the download
    signal.signal(signal.SIGTERM, sigterm_handler)
    # ctrl-c
    signal.signal(signal.SIGINT, sigint_handler)
    fbuild_ids = "build_ids"
    cachedir = None
    tmpdir = None
    keeprpms = False
    noninteractive = False
    b_ids = []

    # localization
    init_gettext()

    ABRT_VERBOSE = os.getenv("ABRT_VERBOSE")
    if (ABRT_VERBOSE):
        try:
            verbose = int(ABRT_VERBOSE)
        except:
            pass

    progname = os.path.basename(sys.argv[0])
    help_text = _("Usage: %s [-i <build_ids_file>] [--tmpdir=TMPDIR] "
                            "[--cache=CACHEDIR] [--keeprpms]") % progname
    try:
        opts, args = getopt.getopt(sys.argv[1:], "vyhi:", ["help", "cache=",
                                                           "tmpdir=","keeprpms"])
    except getopt.GetoptError, err:
        print str(err) # prints something like "option -a not recognized"
        exit(RETURN_FAILURE)

    for opt, arg in opts:
        if opt in ("-h", "--help"):
            print help_text
            exit(0)
        elif opt == "-v":
            verbose += 1
        elif opt == "-y":
            noninteractive = True
        elif opt == "-i":
            fbuild_ids = arg
        elif opt in ("--cache"):
            cachedir = arg
        elif opt in ("--tmpdir"):
            tmpdir = arg
        elif opt in ("--keeprpms"):
            keeprpms = True

    if not cachedir:
        cachedir = "/var/cache/abrt-di"
    if not tmpdir:
        # security people prefer temp subdirs in app's private dir, like /var/run/abrt
        # for now, we use /tmp...
        tmpdir = "/tmp/abrt-tmp-debuginfo-%s.%u" % (time.strftime("%Y-%m-%d-%H:%M:%S"), os.getpid())

    try:
        fin = open(fbuild_ids, "r")
    except IOError, ex:
        print _("Can't open %s: %s" % (fbuild_ids, ex))
        exit(RETURN_FAILURE)
    for line in fin.readlines():
        b_ids.append(line.strip('\n'))

    if not b_ids:
        exit(RETURN_FAILURE)

    missing = filter_installed_debuginfos(b_ids, cachedir)
    if missing:
        log2(missing)
        print _("Coredump references %u debuginfo files, %u of them are not installed") % (len(b_ids), len(missing))
        downloader = DebugInfoDownload(cache=cachedir, tmp=tmpdir)
        try:
            result = downloader.download(missing)
        except Exception, ex:
            error_msg_and_die("Can't download debuginfos: %s", ex)
        missing = filter_installed_debuginfos(b_ids, cachedir)
        for bid in missing:
            print _("Missing debuginfo file: %s") % bid
        exit(result)

    print _("All %u debuginfo files are available") % len(b_ids)
    exit(RETURN_OK)
