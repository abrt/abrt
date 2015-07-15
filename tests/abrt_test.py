#!/usr/bin/python3
# -*- coding: utf-8 -*-
import unittest
import sys
import rpm
from subprocess import Popen, PIPE
import os
import gobject

PROGNAME = "abrt_test"

sys.path.append("../src/gui")
import CCDBusBackend

def info_msg(fmt, *args):
    sys.stdout.write("%s: %s\n" % (PROGNAME, fmt % args))

def log1(message):
    pass


CRASH = "crash"
TIMEOUT = "timeout"

# prevent creating daemon twice
# otherwise we get can't create signal..
daemon = None
def get_daemon():
    global daemon
    if not daemon:
        daemon = CCDBusBackend.DBusManager()
    return daemon

class TestCrash(unittest.TestCase):
    def setUp(self):
        #Test.__init__(self)
        #print "setUp TestCrash"
        self.daemon = get_daemon()
        self.package_nvr = None
        self.timeout = None
        self.result = None
        self.timeout = None
        self.loop = gobject.MainLoop()
        self.crash_signal = self.daemon.connect("crash", self.on_crash)


    def get_package(self, filename):
        ts = rpm.TransactionSet()
        mi = ts.dbMatch(rpm.RPMTAG_BASENAMES, filename)
        for h in mi:
            package = h['nvr']
            log1("Killing %s from package %s" % (filename, package))
            return package

    def on_timeout(self, test, critical):
        #info_msg("Test has timed out")
        if critical:
            #info_msg("[ FAILED ]")
            self.result = TIMEOUT
        self.loop.quit()

    def on_crash(self, daemon, crashed_package_nvr, uuid, uid):
        if self.timeout:
            #print "removing source id: %i" % self.timeout
            gobject.source_remove(self.timeout)

        log1("got crash signal")
        self.loop.quit()
        self.assertEqual(self.package_nvr, crashed_package_nvr,
                        "expected: %s, got: %s" % (crashed_package_nvr, self.package_nvr))
        self.result = CRASH

class TestCompiled(TestCrash):
    #def __init__(self):
        #TestCrash.__init__(self)

    def setUp(self):
        #info_msg("C/C++ crash detection")
        TestCrash.setUp(self)

    def kill_sleep(self):
        # kill runs sleep 100 and sends a SEGV to it"
        self.timeout = gobject.timeout_add(5000, self.on_timeout, "c/c++", True)
        app_to_kill = "/bin/sleep"
        self.package_nvr = self.get_package(app_to_kill)
        pid = Popen([app_to_kill,"100"], stdout=PIPE, bufsize=-1).pid
        os.kill(pid, 11)
        return False

    def test_c_cpp_crash(self):
        gobject.timeout_add(5, self.kill_sleep)
        self.loop.run()
        self.assertEqual(self.result, CRASH, "expected: %s, got: %s" % (CRASH, self.result))

    def test_timeout_on_repeat(self):
        #info_msg("testing repeated crash - should timedout")
        gobject.timeout_add(5, self.kill_sleep)
        self.loop.run()
        self.assertEqual(self.result, TIMEOUT, "expected: %s, got: %s" % (TIMEOUT, self.result))

    #def runTest(self):
    #    info_msg("Testing C/C++")

    def tearDown(self):
        self.daemon.disconnect(self.crash_signal)

class TestPython(TestCrash):
    def test_catch_exception(self):
        pass

    def test_pyhook(self):
    # TODO: how to test the hook with default config?
    # we need broken signed package...
        pass

if __name__ == "__main__":
    try:
        # finds every instance of class TestCompiled and run every method
        # called test_*
        suite = unittest.TestLoader().loadTestsFromTestCase(TestCompiled)
        unittest.TextTestRunner(verbosity=2).run(suite)
        suite = unittest.TestLoader().loadTestsFromTestCase(TestPython)
        unittest.TextTestRunner(verbosity=2).run(suite)
    except KeyboardInterrupt:
        sys.exit()
