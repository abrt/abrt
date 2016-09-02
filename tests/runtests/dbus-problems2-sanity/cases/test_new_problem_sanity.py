#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import os
import dbus

import abrt_p2_testing
from abrt_p2_testing import (wait_for_task_new_problem,
                             get_huge_file_path,
                             create_fully_initialized_problem,
                             open_fd,
                             Problems2Entry,
                             get_dbus_limit_elements_count,)


class TestNewProblemSanity(abrt_p2_testing.TestCase):

    def setUp(self):
        self.p2_entry_path = None
        self.p2_entry_root_path = None

    def tearDown(self):
        if self.p2_entry_path:
            self.p2.DeleteProblems([self.p2_entry_path])

        if self.p2_entry_root_path:
            self.root_p2.DeleteProblems([self.p2_entry_root_path])

    def test_fake_binary_type(self):
        with open("/tmp/fake_type", "w") as type_file:
            type_file.write("CCpp")

        with open("/tmp/fake_type", "r") as type_file:
            description = {"analyzer": "problems2testsuite_analyzer",
                           "reason": "Application has been killed",
                           "backtrace": "die()",
                           "duphash": "FAKE_BINARY_TYPE",
                           "uuid": "FAKE_BINARY_TYPE",
                           "executable": "/usr/bin/foo",
                           "type": dbus.types.UnixFd(type_file)}

            task_path = self.p2.NewProblem(description, 0x1)

            self.assertRaisesProblems2Exception(
                    "Failed to create "
                    "new problem directory: Element 'type' must be of 's' "
                    "D-Bus type",
                    wait_for_task_new_problem, self, self.bus, task_path)

    def test_not_allowed_elements(self):
        description = {"analyzer": "problems2testsuite_analyzer",
                       "type": "Kerneloops",
                       "reason": "Application has been killed",
                       "duphash": "NOT_ALLOWED_ELEMENTS",
                       "uuid": "NOT_ALLOWED_ELEMENTS",
                       "backtrace": "Machine Check Exception: fake"}

        task_path = self.p2.NewProblem(description, 0x1)

        self.assertRaisesProblems2Exception(
                "Failed to create new "
                "problem directory: You are not allowed to create element "
                "'type' containing 'Kerneloops'",
                wait_for_task_new_problem, self, self.bus, task_path)

        task_root_path = self.root_p2.NewProblem(description, 0x1)

        self.p2_entry_root_path = wait_for_task_new_problem(self,
                                                            self.root_bus,
                                                            task_root_path)

        self.assertTrue(self.p2_entry_root_path,
                        "root is not allowed to create type=CCpp")

    def test_real_problem(self):
        self.p2_entry_path = create_fully_initialized_problem(self, self.p2)
        self.assertTrue(self.p2_entry_path,
                        "Failed to return ID of the new problem")

    def test_new_problem_sanitized_uid(self):
        description = {"analyzer": "problems2testsuite_analyzer",
                       "type": "sanitized-uid",
                       "uid": "0",
                       "reason": "Application has been killed",
                       "duphash": "SANITIZED_UID",
                       "backtrace": "die()",
                       "executable": "/usr/bin/foo"}

        task_path = self.p2.NewProblem(description, 0x1)
        self.p2_entry_path = wait_for_task_new_problem(self,
                                                       self.bus,
                                                       task_path)

        self.assertTrue(self.p2_entry_path,
                        "Failed to create problem with uid 0")

        p2e = Problems2Entry(self.bus, self.p2_entry_path)
        self.assertEqual(os.geteuid(), p2e.getproperty("UID"), "Sanitized UID")

    def test_new_problem_sane_default_elements(self):
        description = {}

        task_path = self.p2.NewProblem(description, 0x1)
        self.p2_entry_path = wait_for_task_new_problem(self, self.bus,
                                                       task_path)
        self.assertTrue(self.p2_entry_path,
                        "Failed to create problem without elements")

        p2e = Problems2Entry(self.bus, self.p2_entry_path)
        self.assertEqual("libreport", p2e.getproperty("Type"), "Created type")
        self.assertTrue(p2e.getproperty("UUID"), "Created UUID")

        resp = p2e.ReadElements(["analyzer"], 0)
        if self.assertIn("analyzer", resp, "Created analyzer element"):
            self.assertEqual("libreport", resp["analyzer"])

    def test_new_problem_elements_count_limit(self):
        too_many_elements = dict()
        for i in range(get_dbus_limit_elements_count() + 1):
            too_many_elements[str(i)] = str(i)

        task_path = self.p2.NewProblem(too_many_elements, 0x1)
        self.assertRaisesProblems2Exception(
                "Failed to create "
                "new problem directory: Too many elements",
                wait_for_task_new_problem, self, self.bus, task_path)

    def test_new_problem_data_size_limit(self):
        huge_file_path = get_huge_file_path()

        with open(huge_file_path, "r") as huge_file:
            description = {"analyzer": "problems2testsuite_analyzer",
                           "reason": "Application has been killed",
                           "backtrace": "die()",
                           "duphash": "NEW_PROBLEM_DATA_SIZE",
                           "uuid": "NEW_PROBLEM_DATA_SIZE",
                           "huge_file": dbus.types.UnixFd(huge_file),
                           "executable": "/usr/bin/foo",
                           "type": "abrt-problems2-sanity"}

            task_path = self.p2.NewProblem(description, 0x1)
            self.assertRaisesProblems2Exception(
                    "Failed to "
                    "create new problem directory: Problem data is too big",
                    wait_for_task_new_problem, self, self.bus, task_path)

    def test_non_readable_filedescriptor(self):
        with open_fd("/etc/passwd", os.O_PATH) as fd:
            description = {"analyzer": "problems2testsuite_analyzer",
                           "reason": "Application has been killed",
                           "backtrace": "die()",
                           "duphash": "NOT_READABLE_FD",
                           "uuid": "NOT_READABLE_FD",
                           "passwd": dbus.types.UnixFd(fd),
                           "executable": "/usr/bin/foo",
                           "type": "abrt-problems2-sanity"}

            task_path = self.p2.NewProblem(description, 0x1)
            self.assertRaisesProblems2Exception(
                    "Failed to create new "
                    "problem directory: Failed to set file file descriptor of "
                    "the 'passwd' item non-blocking",
                    wait_for_task_new_problem, self, self.bus, task_path)

    def test_pipes(self):
        rp, wp = os.pipe()
        try:
            description = {"analyzer": "problems2testsuite_analyzer",
                           "reason": "Application has been killed",
                           "backtrace": "die()",
                           "duphash": "NON_BLOCKING_OPERATIONS",
                           "uuid": "NON_BLOCKING_OPERATIONS",
                           "pipe": dbus.types.UnixFd(rp),
                           "executable": "/usr/bin/foo",
                           "type": "abrt-problems2-sanity"}

            task_path = self.p2.NewProblem(description, 0x1)
            self.assertRaisesProblems2Exception(
                    "Failed to create new "
                    "problem directory: Failed to save data of passed file "
                    "descriptor",
                    wait_for_task_new_problem, self, self.bus, task_path)

            os.write(wp, b"Epic success!")
            os.close(wp)
            wp = -1

            task_path = self.p2.NewProblem(description, 0x1)
            self.p2_entry_path = wait_for_task_new_problem(self, self.bus,
                                                           task_path)
            entry = Problems2Entry(self.bus, self.p2_entry_path)

            data = entry.ReadElements(["pipe"], 0x0)

            self.assertIn("pipe", data)
            self.assertEqual(data["pipe"], "Epic success!")
        finally:
            os.close(rp)
            if wp != -1:
                os.close(wp)


if __name__ == "__main__":
    abrt_p2_testing.main(TestNewProblemSanity)
