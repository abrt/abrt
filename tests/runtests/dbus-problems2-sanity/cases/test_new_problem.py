#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import os

import abrt_p2_testing

class TestNewProblem(abrt_p2_testing.TestCase):

    def setUp(self):
        self.task = None
        self.task_running = 0
        self.task_stopped = 0
        self.task_done = 0

    def tearDown(self):
        pass

    def handle_properties_changed_signal(self, interface, changed, invalidated):
        status = self.task.getproperty("Status")
        self.assertEqual(status, changed["status"])

        if changed["status"] == 1:
            self.task_running += 1
        elif changed["status"] == 2:
            self.task_stopped += 1

            details = self.task.getproperty("Details")

            self.assertIn("NewProblem.TemporaryEntry", details)
            self.assertRegexpMatches(details["NewProblem.TemporaryEntry"], "/org/freedesktop/Problems2/Entry/.+");

            self.task.Start(dict())
        elif changed["status"] == 3:
            self.interrupt_waiting()
            self.fail("Task has been canceled")
        elif changed["status"] == 4:
            self.interrupt_waiting()
            self.fail(msg="Task has failed")
        elif changed["status"] == 5:
            self.interrupt_waiting()
            self.task_done += 1
        else:
            self.fail(msg="Unexpected status code %s" % (str(changed["status"])))

    def test_new_problem(self):
        session_path = self.p2.GetSession()

        task_path = None
        with abrt_p2_testing.create_fully_initialized_details(True) as description:
            task_path = self.p2.NewProblem(description, 0x1 | 0x2)

        self.assertRegexpMatches(task_path, session_path + "/Task/.+");

        self.task = abrt_p2_testing.Problems2Task(self.bus, task_path)
        self.task.getobjectproperties().connect_to_signal("PropertiesChanged", self.handle_properties_changed_signal)

        status = self.task.getproperty("Status")
        self.assertEqual(0, status);

        details = self.task.getproperty("Details")
        self.assertDictEqual(details, dict())

        self.task.Start(dict())
        self.wait_for_signals(["PropertiesChanged"])

        self.assertEqual(self.task_running, 2)
        self.assertEqual(self.task_stopped, 1)
        self.assertEqual(self.task_done, 1)

        results, code = self.task.Finish()

        self.assertEqual(0, code)

        self.assertIn("NewProblem.Entry", results)
        self.assertRegexpMatches(results["NewProblem.Entry"], "/org/freedesktop/Problems2/Entry/.+");

        self.p2.DeleteProblems([str(results["NewProblem.Entry"])])


if __name__ == "__main__":
    abrt_p2_testing.main(TestNewProblem)
