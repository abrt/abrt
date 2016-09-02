#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import abrt_p2_testing
from abrt_p2_testing import (wait_for_task_new_problem)


class TestDuplicates(abrt_p2_testing.TestCase):

    def setUp(self):
        self.p2_entry_path = None
        self.p2_entry_duplicate_path = None

    def tearDown(self):
        pass
        if self.p2_entry_path:
            self.p2.DeleteProblems([self.p2_entry_path])

        if self.p2_entry_duplicate_path:
            self.p2.DeleteProblems([self.p2_entry_duplicate_path])

    def test_duplicates(self):
        description = {"analyzer": "problems2testsuite_analyzer",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "duphash": "NEW_PROBLEM_DUPLICATES",
                       "uuid": "NEW_PROBLEM_DUPLICATES",
                       "executable": "/usr/bin/true",
                       "type": "abrt-problems2-dupes"}

        task_path = self.p2.NewProblem(description, 0x1)
        self.p2_entry_path = wait_for_task_new_problem(self,
                                                       self.bus,
                                                       task_path)

        task_duplicate_path = self.p2.NewProblem(description, 0x1)

        self.p2_entry_duplicate_path = wait_for_task_new_problem(
                                                         self,
                                                         self.bus,
                                                         task_duplicate_path)

        self.assertEqual(self.p2_entry_path, self.p2_entry_duplicate_path)
        self.p2_entry_duplicate_path = None


if __name__ == "__main__":
    abrt_p2_testing.main(TestDuplicates)
