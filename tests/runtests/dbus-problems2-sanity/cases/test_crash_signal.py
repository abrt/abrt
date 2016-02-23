#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import os
import time

import abrt_p2_testing
from abrt_p2_testing import (create_problem, Problems2Entry, authorize_session)


class TestCrashSanity(abrt_p2_testing.TestCase):

    def setUp(self):
        self.p2.connect_to_signal("Crash", self.handle_crash)
        self.root_p2.connect_to_signal("Crash", self.handle_crash)

        self.crash_signal_occurrences = []
        self.p2_entry_path = None
        self.p2_entry_root_path = None

    def tearDown(self):
        if self.p2_entry_path:
            self.p2.DeleteProblems([self.p2_entry_path])

        if self.p2_entry_root_path:
            self.root_p2.DeleteProblems([self.p2_entry_root_path])

    def test_user_crash_signal(self):
        uuid, duphash = create_problem(self, self.p2, bus=self.bus, wait=False)

        self.loop_counter += 1
        self.wait_for_signals(["Crash"])

        self.assertEqual(len(self.crash_signal_occurrences),
                         2,
                         "Crash signal wasn't emitted")

        self.assertEqual(os.geteuid(),
                         self.crash_signal_occurrences[0][1],
                         "Crash signal was emitted with wrong UID")

        self.p2_entry_path = self.crash_signal_occurrences[0][0]
        p2e = Problems2Entry(self.root_bus, self.p2_entry_path)
        self.assertEqual(uuid, p2e.getproperty("UUID"))
        self.assertEqual(duphash, p2e.getproperty("Duphash"))

    def test_foreign_crash_signal(self):
        with authorize_session(self) as session:
            uuid, duphash = create_problem(self,
                                           self.root_p2,
                                           bus=self.root_bus,
                                           wait=False)

            self.loop_counter += 1
            self.wait_for_signals(["Crash"])

            self.assertEqual(len(self.crash_signal_occurrences),
                             2,
                             "Crash signal for root's problem wasn't emitted")

            self.assertEqual(0,
                             self.crash_signal_occurrences[0][1],
                             "Crash signal was emitted with wrong UID")

            self.p2_entry_root_path = self.crash_signal_occurrences[0][0]
            p2e_root = Problems2Entry(self.root_bus, self.p2_entry_root_path)
            self.assertEqual(uuid, p2e_root.getproperty("UUID"))
            self.assertEqual(duphash, p2e_root.getproperty("Duphash"))

    def test_foreign_crash_signal_not_authorized(self):
        uuid, duphash = create_problem(self,
                                       self.root_p2,
                                       bus=self.root_bus,
                                       wait=False)

        # This must timeout - the signal should be delivered only to
        # root's session.
        self.loop_counter += 1
        self.wait_for_signals(["Crash"])

        self.assertEqual(len(self.crash_signal_occurrences),
                         1,
                         "Crash signal for root's problem wasn't emitted")

        self.assertEqual(0,
                         self.crash_signal_occurrences[0][1],
                         "Crash signal was emitted with wrong UID")

        self.p2_entry_root_path = self.crash_signal_occurrences[0][0]
        p2e_root = Problems2Entry(self.root_bus, self.p2_entry_root_path)
        self.assertEqual(uuid, p2e_root.getproperty("UUID"))
        self.assertEqual(duphash, p2e_root.getproperty("Duphash"))

    def test_user_crash_signal_duplicate(self):
        description = {"analyzer": "problems2testsuite_analyzer",
                       "type": "problems2testsuite_type",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "executable": "/usr/bin/foo"}

        origin = create_problem(self, self.p2, bus=self.bus,
                                       description=description, wait=True)

        time.sleep(30)

        uuid, duphash = create_problem(self, self.p2, bus=self.bus,
                                       description=description, wait=False)

        self.loop_counter += 1
        self.wait_for_signals(["Crash"])

        self.assertEqual(len(self.crash_signal_occurrences),
                         2,
                         "Duplicate Crash signal wasn't emitted")

        self.p2_entry_path = self.crash_signal_occurrences[1][0]
        self.assertEqual(origin,
                         self.p2_entry_path,
                         "Crash signal was emitted with wrong UID")
        self.assertEqual(os.geteuid(),
                         self.crash_signal_occurrences[1][1],
                         "Crash signal was emitted with wrong UID")

        p2e = Problems2Entry(self.root_bus, self.p2_entry_path)
        self.assertEqual(uuid, p2e.getproperty("UUID"))
        self.assertEqual(duphash, p2e.getproperty("Duphash"))
        self.assertEqual(2, p2e.getproperty("Count"))


if __name__ == "__main__":
    abrt_p2_testing.main(TestCrashSanity)
