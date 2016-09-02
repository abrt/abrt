#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import logging
import abrt_p2_testing
from abrt_p2_testing import (authorize_session,
                             create_problem,
                             Problems2Entry,
                             DBUS_ERROR_ACCESS_DENIED_READ,
                             DBUS_ERROR_ACCESS_DENIED_DELETE)


class TestForeignProblems(abrt_p2_testing.TestCase):

    def setUp(self):
        logging.debug("Creating user problem")
        self.p2_entry_path = create_problem(self,
                                            self.p2,
                                            bus=self.bus,
                                            wait=True)

        logging.debug("Creating root problem")
        self.p2_entry_root_path = create_problem(self,
                                                 self.root_p2,
                                                 bus=self.root_bus,
                                                 wait=True)
        logging.debug("Problems created")

    def tearDown(self):
        self.p2.DeleteProblems([self.p2_entry_path])
        self.root_p2.DeleteProblems([self.p2_entry_root_path])

    def test_get_problems(self):
        p = self.p2.GetProblems(0, dict())

        self.assertNotEqual(0, len(p), "no problems")
        self.assertIn(self.p2_entry_path, p, "missing our problem")
        self.assertNotIn(self.p2_entry_root_path,
                         p,
                         "accessible private problem")

    def test_get_foreign_problems(self):
        p = self.p2.GetProblems(0x1, dict())

        self.assertNotEqual(0, len(p), "no problems")
        self.assertNotIn(self.p2_entry_path, p)
        self.assertIn(self.p2_entry_root_path, p)

    def test_get_foreign_problem(self):
        with authorize_session(self):
            p = self.p2.GetProblems(0, dict())

            self.assertNotEqual(0, len(p), "no problems")
            self.assertIn(self.p2_entry_path, p, "missing our problem")
            self.assertIn(self.p2_entry_root_path,
                          p,
                          "missing private problem")

            p = self.p2.GetProblemData(self.p2_entry_root_path)

            self.assertEqual("0", p["uid"][2], "invalid UID")

            p2_entry = Problems2Entry(self.bus, self.p2_entry_root_path)
            self.assertEqual("Application has been killed",
                             p2_entry.getproperty("Reason"),
                             "Properties are accessible")

    def test_foreign_problem_not_accessible(self):
        p = self.p2.GetProblems(0, dict())

        self.assertNotEqual(0, len(p), "no problems")
        self.assertIn(self.p2_entry_path, p, "missing our problem")
        self.assertNotIn(self.p2_entry_root_path,
                         p,
                         "accessible private problem")

        self.assertRaisesDBusError(DBUS_ERROR_ACCESS_DENIED_READ,
                                   self.p2.GetProblemData,
                                   self.p2_entry_root_path)

        self.assertRaisesDBusError(DBUS_ERROR_ACCESS_DENIED_DELETE,
                                   self.p2.DeleteProblems,
                                   [self.p2_entry_root_path])

        p2_entry = Problems2Entry(self.bus, self.p2_entry_root_path)

        self.assertRaisesDBusError(DBUS_ERROR_ACCESS_DENIED_READ,
                                   p2_entry.getproperty, "Reason")


if __name__ == "__main__":
    abrt_p2_testing.main(TestForeignProblems)
