# vim: set makeprg=python3-flake8\ %
import os
import sys
import dbus
import dbus.service
import dbus.exceptions
import logging
import unittest
import random
import string
import time
import subprocess
from dbus.mainloop.glib import DBusGMainLoop
from contextlib import contextmanager
from gi.repository import GLib

BUS_NAME = "org.freedesktop.problems"

DBUS_ERROR_BAD_ADDRESS = (
    "org.freedesktop.DBus.Error.BadAddress: Requested Entry does not exist")
DBUS_ERROR_ACCESS_DENIED_READ = (
    "org.freedesktop.DBus.Error.AccessDenied: "
    "You are not authorized to access the problem")
DBUS_ERROR_ACCESS_DENIED_DELETE = (
    "org.freedesktop.DBus.Error.AccessDenied: "
    "You are not authorized to delete the problem")


class PolkitAuthenticationAgent(dbus.service.Object):
    def __init__(self, bus, subject_bus_name):
        self._object_path = '/org/freedesktop/PolicyKit1/AuthenticationAgent'
        self._replies = list()

        start_time = "0"
        with open("/proc/self/stat") as stat:
            tokens = stat.readline().split(" ")
            start_time = tokens[21]

        self._bus = bus
        self._subject = ('unix-process',
                         {'pid': dbus.types.UInt32(os.getpid()),
                          'start-time': dbus.types.UInt64(int(start_time))})

        self._authority_proxy = None
        self._authority = None

        dbus.service.Object.__init__(self, self._bus, self._object_path)

    def register(self):
        if self._authority is not None:
            logging.error("Polkit AuthenticationAgent : Already registered")
            return

        proxy = self._bus.get_object('org.freedesktop.PolicyKit1',
                                     '/org/freedesktop/PolicyKit1/Authority')

        authority = dbus.Interface(
                        proxy,
                        dbus_interface='org.freedesktop.PolicyKit1.Authority')

        authority.RegisterAuthenticationAgent(self._subject,
                                              "en_US",
                                              self._object_path)

        logging.debug("Polkit AuthenticationAgent registered")

        self._authority_proxy = proxy
        self._authority = authority

    def unregister(self):
        if self._authority is None:
            logging.error("Polkit AuthenticationAgent : Not registered")
            return

        self._authority.UnregisterAuthenticationAgent(self._subject,
                                                      self._object_path)

        logging.debug("Polkit AuthenticationAgent unregistered")

        self._authority_proxy = None
        self._authority = None

    def set_replies(self, replies):
        self._replies = replies

    def _get_authorization_reply(self, message):
        if len(self._replies) == 0:
            logging.warning("Polkit AuthenticationAgent: no reply registered")
            return False

        cb = self._replies.pop(0)
        if isinstance(cb, bool):
            return cb

        try:
            return cb(message)
        except dbus.exceptions.DBusException as ex:
            logging.debug("Polkit AuthenticationAgent: "
                          "callback raised an DBusException: %s" % (str(ex)))
            raise ex
        except Exception as ex:
            logging.exception(str(ex))

        return False

    @dbus.service.method(
            dbus_interface="org.freedesktop.PolicyKit1.AuthenticationAgent",
            in_signature='sssa{ss}saa{sa{sv}}', out_signature='')
    def BeginAuthentication(self, action_id, message, icon_name, details,
                            cookie, identities):
        # all Exceptions in this function are silently ignore
        logging.debug("Polkit AuthenticationAgent: BeginAuthentication : %s"
                      % (cookie))

        if not self._get_authorization_reply(message):
            logging.debug("Dismissed the authorization request")
            raise dbus.exceptions.DBusException(
                    "org.freedesktop.PolicyKit1.Error.Cancelled")

        logging.debug("Acknowledged the authorization request")
        self._authority.AuthenticationAgentResponse2(0, cookie, identities[0])

    @dbus.service.method(
            dbus_interface="org.freedesktop.PolicyKit1.AuthenticationAgent",
            in_signature='s',
            out_signature='')
    def CancelAuthentication(self, cookie):
        # all Exceptions in this function are silently ignore
        logging.warning("Cancel %s" % (cookie))


@contextmanager
def start_polkit_agent(bus, subject_bus_name):
    pk_agent = PolkitAuthenticationAgent(bus, subject_bus_name)
    pk_agent.register()
    yield pk_agent
    pk_agent.unregister()


class Problems2Exception(Exception):
    """Base exception for Problems2 test
    """

    pass


class Problems2ExceptionTaskFailed(Problems2Exception):
    """Exception raised when New Problem tasks fails
    """

    pass


class _Problems2Object(object):

    def __init__(self, bus, obj_path, interface):
        obj_proxy = bus.get_object(BUS_NAME, obj_path)

        self._properties = dbus.Interface(
                            obj_proxy,
                            dbus_interface="org.freedesktop.DBus.Properties")

        self._interface = interface
        self._obj = dbus.Interface(obj_proxy, dbus_interface=interface)

    def __getattribute__(self, name):
        try:
            return object.__getattribute__(self, name)
        except AttributeError:
            obj = object.__getattribute__(self, "_obj")
            return obj.get_dbus_method(name)

    def getobject(self):
        return object.__getattribute__(self, "_obj")

    def getobjectproperties(self):
        return object.__getattribute__(self, "_properties")

    def getproperty(self, name):
        properties = object.__getattribute__(self, "_properties")
        interface = object.__getattribute__(self, "_interface")
        return properties.Get(interface, name)


class Problems2Entry(_Problems2Object):

    def __init__(self, bus, path):
        super(Problems2Entry, self).__init__(
                                        bus,
                                        path,
                                        "org.freedesktop.Problems2.Entry")


class Problems2Session(_Problems2Object):

    def __init__(self, bus, path):
        super(Problems2Session, self).__init__(
                                        bus,
                                        path,
                                        "org.freedesktop.Problems2.Session")


class Problems2Task(_Problems2Object):

    def __init__(self, bus, path):
        super(Problems2Task, self).__init__(
                                        bus,
                                        path,
                                        "org.freedesktop.Problems2.Task")


class TestCase(unittest.TestCase):

    # TODO -> how this works ???
    @classmethod
    def setUpClass(self):
        if os.getuid() != 0:
            print("Run this test under root!")
            sys.exit(1)

        if len(sys.argv) < 2:
            print("Pass an uid of non-root user as the first argument!")
            sys.exit(1)

        # logging.getLogger().setLevel(logging.DEBUG)

        non_root_uid = int(sys.argv[1])

        DBusGMainLoop(set_as_default=True)

        self.loop = GLib.MainLoop()
        self.logger = logging.getLogger()

        self.root_bus = dbus.SystemBus(private=True)
        self.root_p2_proxy = self.root_bus.get_object(
                                                BUS_NAME,
                                                '/org/freedesktop/Problems2')
        self.root_p2 = dbus.Interface(
                                    self.root_p2_proxy,
                                    dbus_interface='org.freedesktop.Problems2')

        os.seteuid(non_root_uid)

        self.bus = dbus.SystemBus(private=True)

        self.p2_proxy = self.bus.get_object(BUS_NAME,
                                            '/org/freedesktop/Problems2')

        self.p2 = dbus.Interface(self.p2_proxy,
                                 dbus_interface='org.freedesktop.Problems2')

        self.p2_session = None

        self.ac_signal_occurrences = []
        self.loop_counter = 0
        self.loop_running = False
        self.tm = -1
        self.signals = []
        self.crash_signal_occurrences = []

    def main_loop_start(self, timeout=10000):
        self.loop_counter += 1
        if self.loop_running:
            return

        self.loop_running = True
        self.tm = GLib.timeout_add(timeout, self._kill_loop)
        self.loop.run()

    def _kill_loop(self):
        print("Loop interrupted on timeout: %s" % (str(self.signals)))
        self.interrupt_waiting(emergency=True)
        return False

    def interrupt_waiting(self, emergency=True):
        self.loop_counter -= 1
        if not emergency and self.loop_counter != 0:
            return

        self.loop_counter = 0
        self.loop_running = False
        self.loop.quit()
        if not emergency:
            GLib.Source.remove(self.tm)

    def handle_authorization_changed(self, status):
        if "AuthorizationChanged" not in self.signals:
            return

        logging.debug("Received AuthorizationChanged signal : %d" % (status))

        self.interrupt_waiting(False)
        self.ac_signal_occurrences.append(status)

    def handle_crash(self, entry_path, uid):
        if "Crash" not in self.signals:
            return

        logging.debug("Received Crash signal : UID=%s; PATH=%s"
                      % (uid, entry_path))

        self.interrupt_waiting(False)
        self.crash_signal_occurrences.append((entry_path, uid))

    def wait_for_signals(self, signals, timeout=10000):
        self.signals = signals
        logging.debug("Waiting for signals %s" % (", ".join(signals)))
        self.main_loop_start(timeout=timeout)

    def assertRaisesDBusError(self, error_msg, cb, *args):
        self.assertRaisesRegexp(dbus.exceptions.DBusException,
                                error_msg,
                                cb,
                                *args)

    def assertRaisesProblems2Exception(self, error_msg, cb, *args):
        self.assertRaisesRegexp(Problems2Exception,
                                error_msg,
                                cb,
                                *args)


def main(test_case_class):
    suite = None
    if len(sys.argv) < 3:
        suite = unittest.TestLoader().loadTestsFromTestCase(test_case_class)
    else:
        suite = unittest.TestSuite()
        for test_case_name in sys.argv[2:]:
            suite.addTest(test_case_class(test_case_name))

    results = unittest.TextTestRunner().run(suite)
    sys.exit(int(not results.wasSuccessful()))


def wait_for_hooks(test):
    time.sleep(1)


def wait_for_task_status(test, bus, task_path, status):
    def on_properties_changed(iface, changed, invalidated):
        if changed["status"] == status:
            test.interrupt_waiting()

    task = Problems2Task(bus, task_path)
    task.getobjectproperties().connect_to_signal("PropertiesChanged",
                                                 on_properties_changed)
    test.wait_for_signals(["PropertiesChanged"], 30000)
    test.assertEquals(task.getproperty("Status"), status)

    return task


def wait_for_task_new_problem(test, bus, task_path):
    def on_properties_changed(iface, changed, invalidated):
        if changed["status"] == 1:
            pass
        else:
            test.interrupt_waiting()

    task = Problems2Task(bus, task_path)
    task.getobjectproperties().connect_to_signal("PropertiesChanged",
                                                 on_properties_changed)
    task.Start(dict())

    test.wait_for_signals(["PropertiesChanged"], timeout=50000)

    results, code = task.Finish()
    if "Error.Message" in results:
        raise Problems2ExceptionTaskFailed(results["Error.Message"])

    return results["NewProblem.Entry"]


def create_problem(test, p2, wait=True, description=None, bus=None):
    if description is None:
        description = {"analyzer": "problems2testsuite_analyzer",
                       "type": "problems2testsuite_type",
                       "reason": "Application has been killed",
                       "backtrace": "die()",
                       "executable": "/usr/bin/foo"}

    randomstring = (''.join(
                            (random.SystemRandom()
                             .choice(string.ascii_uppercase + string.digits))
                            for _ in range(16)))

    if "uuid" not in description:
        description["uuid"] = randomstring
    if "duphash" not in description:
        description["duphash"] = randomstring

    p2p = None
    if wait:
        p2t = p2.NewProblem(description, 0x1)
        if bus is None:
            bus = test.bus
        p2p = wait_for_task_new_problem(test, bus, p2t)
    else:
        p2p = (description["uuid"], description["duphash"])
        p2.NewProblem(description, 0x0)

    return p2p


def create_fully_initialized_problem(test, p2,
                                     wait=True, unique=False, bus=None):
    with create_fully_initialized_details(unique) as description:
            return create_problem(test,
                                  p2,
                                  wait=wait,
                                  description=description,
                                  bus=bus)


@contextmanager
def create_fully_initialized_details(unique=False):
    data = "ABRT test case huge file " * 41
    with open("/tmp/hugetext", "w") as hugetext_file:
        # 9000KiB > 8MiB
        for i in range(0, 9000):
            hugetext_file.write(data)

    with open("/tmp/hugetext", "r") as hugetext_file:
        with open("/usr/bin/true", "r") as bintrue_file:
            description = {"analyzer": "problems2testsuite_analyzer",
                           "type": "problems2testsuite_type",
                           "reason": "Application has been killed",
                           "backtrace": "die()",
                           "executable": "/usr/bin/foo",
                           "package": "problems2-1.2-3",
                           "pkg_name": "problems2",
                           "pkg_version": "1.2",
                           "pkg_release": "3",
                           "cmdline": "/usr/bin/foo --blah",
                           "component": "abrt",
                           "reported_to": "ABRT Server: "
                                          "BTHASH=0123456789ABCDEF "
                                          "MSG=test\n"
                                          "Server: URL=http://example.org\n"
                                          "Server: URL=http://case.org\n",
                           "hugetext": dbus.types.UnixFd(hugetext_file),
                           "binary": dbus.types.UnixFd(bintrue_file),
                           "bytes": dbus.types.Array(
                                   bytearray([0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                             0xA, 0xB, 0xC, 0xD, 0xE, 0xF]),
                                   "y")}

            if not unique:
                description["uuid"] = "0123456789ABCDEF"
                description["duphash"] = "FEDCBA9876543210"
            else:
                randomstring = (
                    ''.join((random.SystemRandom()
                             .choice(string.ascii_uppercase + string.digits))
                            for _ in range(16)))

                if "uuid" not in description:
                    description["uuid"] = randomstring
                if "duphash" not in description:
                    description["duphash"] = randomstring

            yield description


def get_dbus_limit_data_size():
    env = os.getenv("ABRT_DBUS_DATA_SIZE_LIMIT")
    if env:
        return int(env)
    return 2 * 1024 * 1024 * 1024


def get_dbus_limit_elements_count():
    env = os.getenv("ABRT_DBUS_ELEMENTS_LIMIT")
    if env:
        return int(env)
    return 100


def get_huge_file_path(size_kb=0):
    if size_kb == 0:
        limit = get_dbus_limit_data_size()
        size_kb = int(limit / 1024)

    huge_file_path = "/var/tmp/abrt.testsuite.huge-file"
    try:
        size = os.path.getsize(huge_file_path)
        if size < size_kb * 1024:
            raise OSError
    except OSError:
        subprocess.call(['dd',
                         'bs=1024',
                         'count=' + str(size_kb),
                         'if=/dev/urandom',
                         'of='+huge_file_path])

    return huge_file_path


def get_authorized_session(test, bus=None, session_path=None):
    if bus is None:
        bus = test.bus

    if session_path is None:
        session_path = test.p2.GetSession()

    p2_session = Problems2Session(bus, session_path)
    p2_session.getobject().connect_to_signal("AuthorizationChanged",
                                             test.handle_authorization_changed)

    with start_polkit_agent(test.root_bus, bus.get_unique_name()) as pk_agent:
        pk_agent.set_replies([True])
        p2_session.Authorize(dict())
        # Two signals -> Accepted, Granted
        test.loop_counter += 1
        test.wait_for_signals(["AuthorizationChanged"])

    return p2_session


@contextmanager
def authorize_session(test, bus=None, session_path=None):
    p2_session = get_authorized_session(test, bus, session_path)
    yield p2_session
    p2_session.Close()


@contextmanager
def open_fd(filename, flags):
    fd = os.open(filename, flags)
    yield fd
    os.close(fd)
