#!/usr/bin/python3
# vim: set makeprg=python3-flake8\ %

import dbus
import time
import logging

import abrt_p2_testing
from abrt_p2_testing import (BUS_NAME,
                             Problems2Session,
                             authorize_session,
                             get_session)


class TestSessionAuthorizationPassing(abrt_p2_testing.TestCase):

    def setUp(self):
        self.another_bus = dbus.SystemBus(private=True)
        self.another_bus.set_exit_on_disconnect(False)

        self.another_p2_proxy = self.another_bus.get_object(
                                    BUS_NAME,
                                    '/org/freedesktop/Problems2')

        self.another_p2 = dbus.Interface(
                                    self.another_p2_proxy,
                                    dbus_interface='org.freedesktop.Problems2')

        self.another_p2_session_path = self.another_p2.GetSession()
        self.another_p2_session = Problems2Session(
                                    self.another_bus,
                                    self.another_p2_session_path)
        logging.debug("Test set up")

    def tearDown(self):
        logging.debug("Tearing down test")

        if self.another_p2_session is not None:
            if self.another_p2_session.getproperty("IsAuthorized"):
                self.another_p2_session.RevokeAuthorization()
                self.wait_for_signals(["AuthorizationChanged"])

        if self.another_bus is not None:
            self.another_bus.close()

        logging.debug("Test teared down")

    def test_successful_authorization(self):
        with authorize_session(self) as p2_session:
            token = p2_session.GenerateToken(0)

            self.another_p2_session.getobject().connect_to_signal(
                    "AuthorizationChanged", self.handle_authorization_changed)

            ret = self.another_p2_session.Authorize(
                    {'problems2.peer-bus': self.bus.get_unique_name(),
                     'problems2.peer-token': token})

            self.assertEqual(0, ret,"Authorization passed")
            self.wait_for_signals(["AuthorizationChanged"])

            logging.debug("Signal processing done")

            self.assertTrue(
                    self.another_p2_session.getproperty("IsAuthorized"),
                    "Token Authorization made Session authorized")

    def test_two_tokens(self):
        with authorize_session(self) as p2_session:
            token1 = p2_session.GenerateToken(20)
            token2 = p2_session.GenerateToken(0)
            self.assertNotEqual(token1, token2)

            self.another_p2_session.getobject().connect_to_signal(
                    "AuthorizationChanged", self.handle_authorization_changed)

            ret = self.another_p2_session.Authorize(
                    {'problems2.peer-bus': self.bus.get_unique_name(),
                     'problems2.peer-token': token2})

            self.assertEqual(0, ret, "Authorization passed")
            self.wait_for_signals(["AuthorizationChanged"])
            logging.debug("Authorization via the second token finished")

            self.assertTrue(
                    self.another_p2_session.getproperty("IsAuthorized"),
                    "Token Authorization made Session authorized")

            self.another_p2_session.RevokeAuthorization()
            self.wait_for_signals(["AuthorizationChanged"])

            ret = self.another_p2_session.Authorize(
                    {'problems2.peer-bus': self.bus.get_unique_name(),
                     'problems2.peer-token': token1})

            self.assertEqual(0, ret, "Authorization passed")
            self.wait_for_signals(["AuthorizationChanged"])
            logging.debug("Authorization via the first token finished")

    def test_unauthorized_session(self):
        p2_session = get_session(self)

        self.assertRaisesDBusError(
                "org.freedesktop.DBus.Error.Failed: Cannot generate token: "
                "Session is not authorized",
                p2_session.GenerateToken, 0)

        p2_session.RevokeAuthorization()

    def test_revoked_authorization_session(self):
        token = None
        with authorize_session(self) as p2_session:
            token = p2_session.GenerateToken(0)

        self.assertRaisesDBusError(
                "org.freedesktop.DBus.Error.AccessDenied: Failed to authorize Session: "
                "Not authorized session cannot pass authorization",
                self.another_p2_session.Authorize,
                {'problems2.peer-bus': self.bus.get_unique_name(),
                 'problems2.peer-token': token})

        self.assertFalse(
                self.another_p2_session.getproperty("IsAuthorized"),
                "Token Authorization with De-authorized Session"
                " made Session authorized")

    def test_expired_token(self):
        with authorize_session(self) as p2_session:
            token = p2_session.GenerateToken(0)
            time.sleep(6)

            self.assertRaisesDBusError(
                "org.freedesktop.DBus.Error.AccessDenied: Failed to authorize Session: "
                "Token has already expired",
                self.another_p2_session.Authorize,
                {'problems2.peer-bus': self.bus.get_unique_name(),
                 'problems2.peer-token': token})

            self.assertFalse(
                    self.another_p2_session.getproperty("IsAuthorized"),
                    "Token Authorization made Session authorized")

    def test_random_token(self):
        with authorize_session(self) as p2_session:
            self.assertRaisesDBusError(
                "org.freedesktop.DBus.Error.AccessDenied: Failed to authorize Session: "
                "No such token",
                self.another_p2_session.Authorize,
                {'problems2.peer-bus': self.bus.get_unique_name(),
                 'problems2.peer-token': "fooblah"})

            self.assertFalse(
                    self.another_p2_session.getproperty("IsAuthorized"),
                    "Token Authorization made Session authorized")

    def test_foreign_user(self):
        root_p2_session_path = self.root_p2.GetSession()
        root_p2_session = get_session(self, self.root_bus, root_p2_session_path)
        token = root_p2_session.GenerateToken(0)

        uname = self.root_bus.get_unique_name()
        self.assertRaisesDBusError(
            "org.freedesktop.DBus.Error.Failed: Failed to authorize Session: "
            "No peer session for bus '%s'" % (uname),
            self.another_p2_session.Authorize,
            {'problems2.peer-bus': uname,
             'problems2.peer-token': token})

        self.assertFalse(
            self.another_p2_session.getproperty("IsAuthorized"),
            "Foreign session authorized foreign session")

    def test_reused_token(self):
        with authorize_session(self) as p2_session:
            token = p2_session.GenerateToken(0)

            ret = self.another_p2_session.Authorize(
                    {'problems2.peer-bus': self.bus.get_unique_name(),
                     'problems2.peer-token': token})

            self.assertEqual(0, ret, "Authorization succeeded")

            self.assertTrue(
                    self.another_p2_session.getproperty("IsAuthorized"),
                    "Token Authorization made Session authorized")

            self.another_p2_session.RevokeAuthorization()
            self.assertRaisesDBusError(
                "org.freedesktop.DBus.Error.AccessDenied: Failed to authorize Session: "
                "No such token",
                self.another_p2_session.Authorize,
                {'problems2.peer-bus': self.bus.get_unique_name(),
                 'problems2.peer-token': token})

    def test_random_bus(self):
        with authorize_session(self) as p2_session:
            token = p2_session.GenerateToken(0)

            self.assertRaisesDBusError(
                "org.freedesktop.DBus.Error.Failed: Failed to authorize Session: "
                "No peer session for bus 'fooblah'",
                self.another_p2_session.Authorize,
                {'problems2.peer-bus': "fooblah",
                 'problems2.peer-token': token})

            self.assertFalse(
                    self.another_p2_session.getproperty("IsAuthorized"),
                    "Token Authorization made Session authorized")


if __name__ == "__main__":
    abrt_p2_testing.main(TestSessionAuthorizationPassing)
