#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import dbus
import time

import abrt_p2_testing
from abrt_p2_testing import (BUS_NAME, get_huge_file_path, Problems2Task)


class TestConcurency(abrt_p2_testing.TestCase):

    def setUp(self):
        self.p2_entry_path = None
        self.p2_entry_root_path = None

    def tearDown(self):
        if self.p2_entry_path:
            self.p2.DeleteProblems([self.p2_entry_path])

        if self.p2_entry_root_path:
            self.root_p2.DeleteProblems([self.p2_entry_root_path])

    def test_new_problem(self):
        self.replies = 4

        huge_file_path = get_huge_file_path()

        buses = list()
        while len(buses) < self.replies:
            huge_file = open(huge_file_path, "r")
            description = {"analyzer": "problems2testsuite_analyzer",
                           "reason": "Application has been killed",
                           "backtrace": "die()",
                           "duphash": "NEW_PROBLEM_DATA_SIZE",
                           "uuid": "NEW_PROBLEM_DATA_SIZE",
                           "huge_file": dbus.types.UnixFd(huge_file),
                           "executable": "/usr/bin/foo",
                           "type": "abrt-problems2-sanity"}

            bus = dbus.SystemBus(private=True)

            p2_proxy = bus.get_object(BUS_NAME,
                                      '/org/freedesktop/Problems2')

            p2 = dbus.Interface(p2_proxy,
                                dbus_interface='org.freedesktop.Problems2')

            buses.append((bus, p2, description, huge_file))

        task_paths = list()
        for bus, p2, description, _ in buses:
            task_paths.append((p2.NewProblem(description, 0x1 | 0x4), bus))

        tasks = list()
        for path, bus in task_paths:
            tasks.append(Problems2Task(bus, path))

        for i in range(0, 60):
            for t in tasks:
                status = t.getproperty("Status")
                if status == 4:
                    results, code = t.Finish()
                    self.assertIn("Error.Message", results)
                    self.assertRegexpMatches(results["Error.Message"],
                                             "Failed to create new problem "
                                             "directory: Problem data is "
                                             "too big")
                    tasks.remove(t)
                elif not status == 1:
                    self.fail("Unexpected task status: %s" % (str(status)))

            if not tasks:
                break
            time.sleep(1)

        self.assertFalse(tasks)

        for _, _, _, huge_file in buses:
            huge_file.close()


if __name__ == "__main__":
    abrt_p2_testing.main(TestConcurency)
