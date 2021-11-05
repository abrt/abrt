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
                    self.assertEqual(message, exp_message)

                # Verify that Authorize returns 2 if there is a pending request
                ret = p2_session.Authorize(dict())
                self.assertEqual(2, ret, "Not-yet finished authorization request")

                # The loop_counter is incremented before we start waiting for
                # signal several lines below. The increment is there because we
                # must be waiting for the signal and also for Polkit
                # authorization. This call will interrupt the waiting for
                # signal.
                self.interrupt_waiting()
                return retval

            # The code below should produce the following sequence of signals:
            # - 1 (PENDING)
            # - 3 (FAILED)
            # - 1 (PENDING)
            # - 0 (GRANTED)
            pk_agent.set_replies([partial(check_pending_authorization, False, "Foo the bars"),
                                  partial(check_pending_authorization, True, None)])

            self.logger.debug("Calling Authorize(): expecting failure")

            # First attempt - this time authorization should fail
            ret = p2_session.Authorize({"message" : "Foo the bars"})
            self.assertEqual(1, ret, "Pending authorization request")

            # Polkit agent will interrupt waiting.
            self.loop_counter += 1

            # Waiting for signals runs a main loop and the polkit agent can
            # handle requests only from a main loop. Therefore, this waiting
            # has two purposes - running main loop and synchronization with
            # abrt-dbus.
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

            # Second attempt - this time authorization should be successful
            self.logger.debug("Calling Authorize(): expecting success")

            ret = p2_session.Authorize(dict())
            self.assertEqual(1, ret, "Pending authorization request")

            # Polkit agent will interrupt waiting.
            self.loop_counter += 1

            # This is also required for Polkig agent, because the method runs
            # main loop which invokes agent's methods.
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

        p2_session.RevokeAuthorization()

        self.wait_for_signals(["AuthorizationChanged"])

        if self.assertTrue(len(self.ac_signal_occurrences) == 5, "Revoked authorization session signal wasn't emitted"):
            self.assertEqual(2, self.ac_signal_occurrences[0], "Revoked authorization session signal value")

        p2_session_path = self.p2.GetSession()
        p2_session = Problems2Session(self.bus, p2_session_path)

        self.assertFalse(p2_session.getproperty("IsAuthorized"), msg = "still authorized")


if __name__ == "__main__":
    abrt_p2_testing.main(TestSession)
