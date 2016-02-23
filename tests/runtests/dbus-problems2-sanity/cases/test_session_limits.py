#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import dbus
import time

from functools import partial

import abrt_p2_testing
from abrt_p2_testing import (Problems2Service,
                             Problems2Session,
                             get_session,
                             get_authorized_session,
                             start_polkit_agent)


class TestSessionLimits(abrt_p2_testing.TestCase):

    def test_open_too_many_sessions(self):
        sessions = dict()
        i = 0
        try:
            while i < 10:
                bus = dbus.SystemBus(private=True)
                p2 = Problems2Service(bus)
                p2s_path = p2.GetSession()
                i += 1
                if p2s_path in sessions:
                    self.fail("got a session owned by another caller,"
                              " run = %d" % (i))
                else:
                    sessions[p2s_path] = (bus, p2)
            self.fail("managed to open %d sessions" % (i))
        except dbus.exceptions.DBusException as ex:
            self.assertEqual("org.freedesktop.DBus.Error.Failed: "
                             "Too many sessions opened",
                             str(ex),
                             "managed to open %d sessions" % (i))
            self.assertEqual(5, i, "unexpected opened sessions limit")

        for k, v in sessions.items():
            p2s = Problems2Session(v[0], k)
            p2s.RevokeAuthorization()

    def test_foreign_session(self):
        root_session_path = self.root_p2.GetSession()

        # Get root's session on user's bus.
        # RevokeAuthorization() method must fail.
        root_session = Problems2Session(self.bus, root_session_path)

        self.assertRaisesDBusError("org.freedesktop.DBus.Error.Failed: "
                                   "Your Problems2 Session is broken. "
                                   "Check system logs for more details.",
                                   root_session.RevokeAuthorization)

        # Get root's session on root's bus. RevokeAuthorization() method
        # should succeed.
        root_session = Problems2Session(self.root_bus, root_session_path)
        root_session.RevokeAuthorization()

    def test_auto_destruct(self):
        # Random bus connection which will be closed
        bus = dbus.SystemBus(private=True)
        bus.set_exit_on_disconnect(False)

        # Otherwise AuthorizationSignal is not emitted
        p2s = get_authorized_session(self, bus)
        bus.close()

        bus = dbus.SystemBus(private=True)
        bus.set_exit_on_disconnect(False)
        p2s = Problems2Session(bus, p2s.getobject().object_path)

        exception_msg = "org.freedesktop.DBus.Error.UnknownMethod: " \
                        "No such interface " \
                        "'org.freedesktop.DBus.Properties' on object " \
                        "at path {0}".format(p2s.getobject().object_path)

        self.assertRaisesDBusError(exception_msg,
                                   p2s.getproperty,
                                   "IsAuthorized")

        bus.close()

    def test_auto_cancel_authorize_request(self):
        for pkagent_reply in [True, False]:
            self.logger.debug("Test with reply: %s " % (pkagent_reply))

            # Random bus connection which will be closed
            bus = dbus.SystemBus(private=True)
            bus.set_exit_on_disconnect(False)

            with start_polkit_agent(self.root_bus,
                                    bus.get_unique_name()) as pk_agent:

                def close_bus(retval, bus, exp_message, message):
                    self.logger.debug("Killing bus: %s"
                                      % (bus.get_unique_name()))
                    bus.close()

                    self.logger.debug("Let abrt-dbus to process "
                                      "disconnected client")
                    time.sleep(1)

                    # The loop_counter is incremented before we start waiting
                    # for signal several lines below. The increment is there
                    # because we must be waiting for the signal and also for
                    # Polkit authorization. This call will interrupt the
                    # waiting for signal.
                    self.interrupt_waiting()
                    return retval

                pk_agent.set_replies([partial(close_bus,
                                              pkagent_reply,
                                              bus,
                                              None)])

                p2s = get_session(self, bus)
                p2s.getobject().connect_to_signal(
                                        "AuthorizationChanged",
                                        self.handle_authorization_changed)

                self.logger.debug("Authorizing own session")

                r = p2s.Authorize(dict())
                self.assertEquals(r, 1, "Session is being authorized")

                self.ac_signal_occurrences = list()

                # Polkit agent will interrupt waiting.
                self.loop_counter += 1

                # Waiting for signals runs a main loop and the polkit agent
                # can handle requests only from a main loop. Therefore, this
                # waiting has two purposes - running main loop and
                # synchronization with abrt-dbus.
                self.wait_for_signals(["AuthorizationChanged"])

                self.assertEqual(len(self.ac_signal_occurrences), 1,
                                "Session emitted a signal")

                self.assertEqual(self.ac_signal_occurrences[0],
                                 1,
                                 "Authorization request was accepted")

                self.logger.debug("Going to check Session object")
                # Give abrt-dbus some time to deal with a disappeared session.
                time.sleep(1)

                self.logger.debug("Opening a temporary DBus connection")
                bus = dbus.SystemBus(private=True)
                bus.set_exit_on_disconnect(False)
                p2s = Problems2Session(bus, p2s.getobject().object_path)

                exception_msg = "org.freedesktop.DBus.Error.UnknownMethod: " \
                                "No such interface " \
                                "'org.freedesktop.DBus.Properties' on object " \
                                "at path {0}".format(p2s.getobject().object_path)

                self.assertRaisesDBusError(exception_msg,
                                           p2s.getproperty,
                                           "IsAuthorized")

                self.logger.debug("Closing the temporary DBus connection")
                bus.close()

if __name__ == "__main__":
    abrt_p2_testing.main(TestSessionLimits)
