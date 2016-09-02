#!/usr/bin/python3

import abrt_p2_testing
from abrt_p2_testing import (wait_for_task_new_problem,
                             DBUS_ERROR_BAD_ADDRESS,)


class TestDeleteProblemsSanity(abrt_p2_testing.TestCase):

    def test_delete_problems(self):
        description = {"analyzer": "problems2testsuite_analyzer",
                       "type": "problems2testsuite_type",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "executable": "/usr/bin/true",
                       "duphash": None,
                       "uuid": None}

        description["duphash"] = description["uuid"] = "DEADBEEF"
        task_one_path = self.p2.NewProblem(description, 0x1)
        one = wait_for_task_new_problem(self, self.bus, task_one_path)

        description["duphash"] = description["uuid"] = "81680083"
        task_two_path = self.p2.NewProblem(description, 0x1)
        two = wait_for_task_new_problem(self, self.bus, task_two_path)

        description["duphash"] = description["uuid"] = "FFFFFFFF"
        task_three_path = self.p2.NewProblem(description, 0x1)
        three = wait_for_task_new_problem(self, self.bus, task_three_path)

        p = self.p2.GetProblems(0, dict())

        self.assertIn(one, p)
        self.assertIn(two, p)
        self.assertIn(three, p)

        self.p2.DeleteProblems([one])

        p = self.p2.GetProblems(0, dict())

        self.assertNotIn(one, p)
        self.assertIn(two, p)
        self.assertIn(three, p)

        self.assertRaisesDBusError(DBUS_ERROR_BAD_ADDRESS,
                                   self.p2.DeleteProblems,
                                   [two, three, one])

        p = self.p2.GetProblems(0, dict())

        self.assertNotIn(one, p)
        self.assertNotIn(two, p)
        self.assertNotIn(three, p)

        self.p2.DeleteProblems([])

        self.assertRaisesDBusError(DBUS_ERROR_BAD_ADDRESS,
                                   self.p2.DeleteProblems,
                                   ["/invalid/path"])

        self.assertRaisesDBusError(DBUS_ERROR_BAD_ADDRESS,
                                   self.p2.DeleteProblems,
                                   ["/org/freedesktop/Problems2/Entry/FAKE"])


if __name__ == "__main__":
    abrt_p2_testing.main(TestDeleteProblemsSanity)
