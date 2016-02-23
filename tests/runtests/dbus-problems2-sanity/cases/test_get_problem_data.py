#!/usr/bin/python3

import os
import dbus
import re

import abrt_p2_testing
from abrt_p2_testing import (create_problem,
                             create_fully_initialized_problem,
                             DBUS_ERROR_ACCESS_DENIED_READ,
                             DBUS_ERROR_BAD_ADDRESS,)

class TestGetProblemDataSanity(abrt_p2_testing.TestCase):

    def setUp(self):
        self.p2_entry_path = create_fully_initialized_problem(self, self.p2)
        self.p2_entry_root_path = create_problem(self, self.root_p2, bus=self.root_bus)

    def tearDown(self):
        if self.p2_entry_path:
            self.p2.DeleteProblems([self.p2_entry_path])

        if self.p2_entry_root_path:
            self.root_p2.DeleteProblems([self.p2_entry_root_path])

    def test_get_problem_data(self):
        self.assertRaisesDBusError(DBUS_ERROR_BAD_ADDRESS,
                self.p2.GetProblemData, "/invalid/path")

        self.assertRaisesDBusError(DBUS_ERROR_BAD_ADDRESS,
                self.p2.GetProblemData, "/org/freedesktop/Problems2/Entry/FAKE")

        self.assertRaisesDBusError(DBUS_ERROR_ACCESS_DENIED_READ,
                self.p2.GetProblemData, self.p2_entry_root_path)

        p = self.p2.GetProblemData(self.p2_entry_path)
        expected = {
            "analyzer"    : (10, len("problems2testsuite_analyzer"), "problems2testsuite_analyzer"),
            "type"        : (10, len("problems2testsuite_type"), "problems2testsuite_type"),
            "reason"      : (22, len("Application has been killed"), "Application has been killed"),
            "backtrace"   : (6, len("die()"), "die()"),
            "executable"  : (10, len("/usr/bin/foo"), "/usr/bin/foo"),
            "hugetext"    : (73, os.path.getsize("/tmp/hugetext"), "/var/spool/abrt/[^/]+/hugetext"),
            "binary"      : (9, os.path.getsize("/usr/bin/true"), "/var/spool/abrt/[^/]+/binary"),
        }

        for k, v in expected.items():
            if self.assertIn(k, p):
                print("Missing %s" % (k))
                continue

            g = p[k]

            self.assertRegexpMatches(g[2], v[2], "invalid contents of '%s'" % (k))
            self.assertEqual(v[1], g[1], "invalid length '%s'" % (k))
            self.assertEqual(v[0], g[0], "invalid flags %s" % (k))


if __name__ == "__main__":
    abrt_p2_testing.main(TestGetProblemDataSanity)
