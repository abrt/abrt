#!/usr/bin/python3

import sys
import dbus
from functools import partial

import abrt_p2_testing
from abrt_p2_testing import start_polkit_agent, BUS_NAME, Problems2Session


class TestSession(abrt_p2_testing.TestCase):

    def test_get_authorize_close(self):
        self.ac_signal_occurrences = []
        p2_session_path = self.p2.GetSession()
        p2_session = Problems2Session(self.bus, p2_session_path)

        self.assertFalse(p2_session.getproperty("IsAuthorized"),
                    "Session is authorized by default")

        p2_session.getobject().connect_to_signal("AuthorizationChanged", self.handle_authorization_changed)

        with start_polkit_agent(self.root_bus, self.bus.get_unique_name()) as pk_agent:
            def check_pending_authorization(retval, exp_message, message):
                self.logger.debug("Calling Authorize(): expecting pending")

                if not exp_message is None:
                    self.assertEquals(message, exp_message)

                ret = p2_session.Authorize(dict())
                self.assertEqual(2, ret, "Not-yet finished authorization request")
                self.interrupt_waiting()
                return retval

            pk_agent.set_replies([partial(check_pending_authorization, False, "Foo the bars"),
                                  partial(check_pending_authorization, True, None)])

            self.logger.debug("Calling Authorize(): expecting failure")

            ret = p2_session.Authorize({"message" : "Foo the bars"})
            self.assertEqual(1, ret, "Pending authorization request")

            self.loop_counter += 1
            self.wait_for_signals(["AuthorizationChanged"])

            if self.assertTrue(len(self.ac_signal_occurrences) == 1, "Pending signal wasn't emitted"):
                self.assertEqual(1, self.ac_signal_occurrences[0], "Pending signal value")

            self.assertFalse(p2_session.getproperty("IsAuthorized"),
                        "Pending authorization request made Session authorized")

            self.wait_for_signals(["AuthorizationChanged"])

            if self.assertTrue(len(self.ac_signal_occurrences) == 2, "Failure signal wasn't emitted"):
                self.assertEqual(3, self.ac_signal_occurrences[1], "Failure signal value")

            self.assertFalse(p2_session.getproperty("IsAuthorized"),
                        "Failed authorization request made Session authorized")

            self.logger.debug("Calling Authorize(): expecting success")

            ret = p2_session.Authorize(dict())
            self.assertEqual(1, ret, "Pending authorization request")

            self.loop_counter += 1
            self.wait_for_signals(["AuthorizationChanged"])

            if self.assertTrue(len(self.ac_signal_occurrences) == 3, "Pending signal 2 wasn't emitted"):
                self.assertEqual(1, self.ac_signal_occurrences[2], "Pending signal 2 value")

            self.assertFalse(p2_session.getproperty("IsAuthorized"),
                        "Pending authorization request 2 made Session authorized")

            self.wait_for_signals(["AuthorizationChanged"])

            if self.assertTrue(len(self.ac_signal_occurrences) == 4, "Authorized signal wasn't emitted"):
                self.assertEqual(0, self.ac_signal_occurrences[3], "Authorized signal value")

            self.assertTrue(p2_session.getproperty("IsAuthorized"),
                        "Authorization request did not make Session authorized")

        p2_session.Close()

        self.wait_for_signals(["AuthorizationChanged"])

        if self.assertTrue(len(self.ac_signal_occurrences) == 5, "Closed session signal wasn't emitted"):
            self.assertEqual(2, self.ac_signal_occurrences[0], "Closed session signal value")

        p2_session_path = self.p2.GetSession()
        p2_session = Problems2Session(self.bus, p2_session_path)

        self.assertFalse(p2_session.getproperty("IsAuthorized"), msg = "still authorized")


if __name__ == "__main__":
    abrt_p2_testing.main(TestSession)
