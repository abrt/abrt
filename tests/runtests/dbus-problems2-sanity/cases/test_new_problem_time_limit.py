#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import time

import abrt_p2_testing
from abrt_p2_testing import (create_problem,
                             )


class TestNewProblemTimeLimit(abrt_p2_testing.TestCase):

    def setUp(self):
        self.problems = list()

    def tearDown(self):
        if self.problems:
            self.p2.DeleteProblems(self.problems)
            self.problems = list()

    def _bunch_of_new_problems(self, upper_limit):
        self.problems = list()
        for i in range(0, upper_limit):
            problem_path = create_problem(self, self.p2, wait=True)
            self.problems.append(problem_path)

    def test_new_problem_time_limit(self):
        self.assertRaisesProblems2Exception(
               "Too many problems have been recently created",
               self._bunch_of_new_problems,
               11)

        self.p2.DeleteProblems(self.problems)

        time.sleep(16)

        self.assertRaisesProblems2Exception(
               "Too many problems have been recently created",
               self._bunch_of_new_problems,
               3)

        self.assertGreaterEqual(len(self.problems),
                                1,
                                "The limit has been restored")


if __name__ == "__main__":
    abrt_p2_testing.main(TestNewProblemTimeLimit)
