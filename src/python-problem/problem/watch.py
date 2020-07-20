import os
import logging

import dbus
import problem


class ProblemWatcher(object):
    ''' New problem signal handler attached to DBus signal

    Use ``auth=True`` if authentication should be attempted for
    new problem that doesn't belong to current user. If not
    set such a problem is ignored.

    '''

    def __init__(self, auth):
        from gi.repository import GObject as gobject
        from dbus.mainloop.glib import DBusGMainLoop

        gobject.threads_init()

        bus = dbus.SystemBus(
            mainloop=DBusGMainLoop(),
            private=True)

        self.bus = bus
        self.proxy = self.bus.get_object('org.freedesktop.problems',
                                         '/org/freedesktop/Problems2')
        self.interface = dbus.Interface(self.proxy,
                                        dbus_interface='org.freedesktop.Problems2')
        self.session = self.interface.GetSession()
        self.auth = auth
        self.callbacks = []

        self.proxy.connect_to_signal('Crash', self._new_problem_handler,
                                     dbus_interface='org.freedesktop.Problems2')

        self.loop = gobject.MainLoop()

    def _new_problem_handler(self, problem_object, uid):
        logging.debug('New problem notification received')
        if int(uid) != os.getuid() and not self.auth:
            logging.debug('Auth disabled, ignoring crash with'
                          ' uid: {0}'.format(uid))
            return

        proxy = self.bus.get_object('org.freedesktop.problems', problem_object)
        ddir = proxy.Get('org.freedesktop.Problems2.Entry', 'ID',
                         dbus_interface=dbus.PROPERTIES_IFACE)
        prob = problem.tools.problemify(ddir, problem.proxies.get_proxy())

        for cb in self.callbacks:
            cb(prob)

    def add_callback(self, fun):
        ''' Add callback to be called when new problem occurs.

        Each callback function receives ``Problem`` instance

        '''

        self.callbacks.append(fun)

    def run(self):
        ''' Start event listener loop '''

        self.loop.run()

    def quit(self):
        ''' Stop event listener loop '''

        self.loop.quit()
