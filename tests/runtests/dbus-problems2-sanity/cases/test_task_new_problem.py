#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import dbus
import time

import abrt_p2_testing
from abrt_p2_testing import (BUS_NAME, Problems2Task, get_huge_file_path,
                             wait_for_task_status)


class TestTaskNewProblem(abrt_p2_testing.TestCase):

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_task_new_task_destroyed_with_session(self):
        pbus = dbus.SystemBus(private=True)
        p2_proxy = pbus.get_object(BUS_NAME,
                                   '/org/freedesktop/Problems2')
        p2 = dbus.Interface(p2_proxy,
                            dbus_interface='org.freedesktop.Problems2')

        description = {"analyzer": "problems2testsuite_analyzer",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "duphash": "TASK_NEW_PROBLEM_SESSION",
                       "uuid": "TASK_NEW_PROBLEM_SESSION",
                       "executable": "/usr/bin/foo",
                       "type": "abrt-problems2-new-destroyed-with-session"}

        task_path = p2.NewProblem(description, 0x1)

        self.bus.get_object(BUS_NAME, task_path)

        pbus.close()

        task = Problems2Task(self.bus, task_path)
        self.assertRaisesDBusError(
            "org.freedesktop.DBus.Error.UnknownMethod: No such interface "
            "'org.freedesktop.DBus.Properties' on object at path " + task_path,
            task.getproperty, "Status")

    def test_task_stopped_task_destroyed_with_session(self):
        pbus = dbus.SystemBus(private=True)
        p2_proxy = pbus.get_object(BUS_NAME,
                                   '/org/freedesktop/Problems2')
        p2 = dbus.Interface(p2_proxy,
                            dbus_interface='org.freedesktop.Problems2')

        description = {"analyzer": "problems2testsuite_analyzer",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "duphash": "TASK_NEW_PROBLEM_SESSION",
                       "uuid": "TASK_NEW_PROBLEM_SESSION",
                       "executable": "/usr/bin/foo",
                       "type": "abrt-problems2-stopped-destroyed-with-session"}

        # Create task, run it and stop after temporary entry is created
        task_path = p2.NewProblem(description, 0x1 | 0x2 | 0x4)

        self.bus.get_object(BUS_NAME, task_path)
        wait_for_task_status(self, pbus, task_path, 2)

        pbus.close()

        task = Problems2Task(self.bus, task_path)
        self.assertRaisesDBusError(
            "org.freedesktop.DBus.Error.UnknownMethod: No such interface "
            "'org.freedesktop.DBus.Properties' on object at path " + task_path,
            task.getproperty, "Status")

    def test_task_done_task_destroyed_with_session(self):
        old_problems = set(self.p2.GetProblems(0, dict()))

        pbus = dbus.SystemBus(private=True)
        p2_proxy = pbus.get_object(BUS_NAME,
                                   '/org/freedesktop/Problems2')
        p2 = dbus.Interface(p2_proxy,
                            dbus_interface='org.freedesktop.Problems2')

        description = {"analyzer": "problems2testsuite_analyzer",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "duphash": "TASK_NEW_PROBLEM_SESSION",
                       "uuid": "TASK_NEW_PROBLEM_SESSION",
                       "executable": "/usr/bin/foo",
                       "type": "abrt-problems2-dd-with-session"}

        # Create task, run it
        task_path = p2.NewProblem(description, 0x1 | 0x4)

        self.bus.get_object(BUS_NAME, task_path)
        wait_for_task_status(self, pbus, task_path, 5)

        pbus.close()

        task = Problems2Task(self.bus, task_path)
        self.assertRaisesDBusError(
            "org.freedesktop.DBus.Error.UnknownMethod: No such interface "
            "'org.freedesktop.DBus.Properties' on object at path " + task_path,
            task.getproperty, "Status")

        new_problems = self.p2.GetProblems(0, dict())
        to_delete = list()
        for p in new_problems:
            if p in old_problems:
                continue
            to_delete.append(p)

        self.assertTrue(to_delete)
        self.p2.DeleteProblems(to_delete)

    def test_task_running_task_destroyed_with_session(self):
        pbus = dbus.SystemBus(private=True)
        p2_proxy = pbus.get_object(BUS_NAME,
                                   '/org/freedesktop/Problems2')
        p2 = dbus.Interface(p2_proxy,
                            dbus_interface='org.freedesktop.Problems2')

        task_path = None
        huge_file_path = get_huge_file_path()
        with open(huge_file_path, "r") as huge_file:
            description = {"analyzer": "problems2testsuite_analyzer",
                           "reason": "Application has been killed",
                           "backtrace": "die()",
                           "duphash": "TASK_NEW_PROBLEM_SESSION",
                           "uuid": "TASK_NEW_PROBLEM_SESSION",
                           "huge_file": dbus.types.UnixFd(huge_file),
                           "executable": "/usr/bin/foo",
                           "type": "abrt-problems2-running-destroyed-with-session"}

            # Create task, run it and stop after temporary entry is created
            task_path = p2.NewProblem(description, 0x1 | 0x2 | 0x4)
            pbus.close()

        task = Problems2Task(self.bus, task_path)
        self.assertRaisesDBusError(
            "org.freedesktop.DBus.Error.UnknownMethod: No such interface "
            "'org.freedesktop.DBus.Properties' on object at path " + task_path,
            task.getproperty, "Status")

        # let abrt-dbus finish its work load
        pbus = dbus.SystemBus(private=True)
        p2_proxy = pbus.get_object(BUS_NAME,
                                   '/org/freedesktop/Problems2')
        p2 = dbus.Interface(p2_proxy,
                            dbus_interface='org.freedesktop.Problems2')

        time.sleep(10)

    def test_accessible_to_own_session_only(self):
        description = {"analyzer": "problems2testsuite_analyzer",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "duphash": "TASK_NEW_PROBLEM_SESSION",
                       "uuid": "TASK_NEW_PROBLEM_SESSION",
                       "executable": "/usr/bin/foo",
                       "type": "abrt-problems2-accessible-to-own-session"}

        # Create task, run it and stop after temporary entry is created
        root_task_path = self.root_p2.NewProblem(description, 0x1 | 0x2 | 0x4)
        root_task = wait_for_task_status(self,
                                         self.root_bus,
                                         root_task_path,
                                         2)

        task = Problems2Task(self.bus, root_task_path)
        self.assertRaisesDBusError(
            "org.freedesktop.DBus.Error.AccessDenied: "
            "The task does not belong to your session",
            task.getproperty, "Status")

        root_task.Cancel(0)

        # let abrt-dbus finish its work load
        time.sleep(1)


if __name__ == "__main__":
    abrt_p2_testing.main(TestTaskNewProblem)
