#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import time

import abrt_p2_testing
from abrt_p2_testing import (wait_for_task_status)


class TestGetProblems(abrt_p2_testing.TestCase):

    def test_get_new_problem(self):
        description = {"analyzer": "problems2testsuite_analyzer",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "duphash": "TASK_NEW_PROBLEM",
                       "uuid": "TASK_NEW_PROBLEM",
                       "executable": "/usr/bin/foo",
                       "type": "abrt-problems2"}

        # Create task, run it and stop after temporary entry is created
        task_path = self.p2.NewProblem(description, 0x1 | 0x2 | 0x4)
        task = wait_for_task_status(self, self.bus, task_path, 2)

        details = task.getproperty("Details")
        self.assertIn("NewProblem.TemporaryEntry", details)

        new_problems = self.p2.GetProblems(0x2, dict())
        self.assertIn(details["NewProblem.TemporaryEntry"], new_problems)

        task.Cancel(0)

        time.sleep(1)

        new_problems = self.p2.GetProblems(0x2, dict())
        self.assertEquals(0, len(new_problems))

    def test_get_new_foreign_problem(self):
        description = {"analyzer": "problems2testsuite_analyzer",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "duphash": "TASK_NEW_PROBLEM",
                       "uuid": "TASK_NEW_PROBLEM",
                       "executable": "/usr/bin/foo",
                       "type": "abrt-problems2"}

        # Create task, run it and stop after temporary entry is created
        task_path = self.root_p2.NewProblem(description, 0x1 | 0x2 | 0x4)
        task = wait_for_task_status(self, self.root_bus, task_path, 2)

        details = task.getproperty("Details")
        self.assertIn("NewProblem.TemporaryEntry", details)

        new_problems = self.p2.GetProblems(0x1 | 0x2, dict())
        self.assertIn(details["NewProblem.TemporaryEntry"], new_problems)

        task.Cancel(0)

        time.sleep(1)

        new_problems = self.p2.GetProblems(0x1 | 0x2, dict())
        self.assertEquals(0, len(new_problems))


if __name__ == "__main__":
    abrt_p2_testing.main(TestGetProblems)
