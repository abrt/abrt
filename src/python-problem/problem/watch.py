import os
import logging

import dbus
import dbus.mainloop.glib
import gobject

import problem

class ProblemWatcher(object):
    ''' New problem signal handler attached to DBus signal

    Use ``auth=True`` if authentication should be attempted for
    new problem that doesn't belong to current user. If not
    set such a problem is ignored.

    '''

    def __init__(self, auth):

        gobject.threads_init()

        bus = dbus.SystemBus(
                mainloop=dbus.mainloop.glib.DBusGMainLoop(),
                private=True)

        self.bus = bus
        self.auth = auth
        self.callbacks = []

        # local context required!?
        # http://rmarko.fedorapeople.org/random/high_five.jpg
        evt_match = self.bus.add_signal_receiver(
            self._new_problem_handler,
            signal_name='Crash', path='/org/freedesktop/problems')

        self.loop = gobject.MainLoop()

    def _new_problem_handler(self, comp, ddir, uid):
        logging.debug('New problem notification received')
        if int(uid) != os.getuid() and not self.auth:
            logging.debug('Auth disabled, ignoring crash with'
                ' uid: {0}'.format(uid))
            return

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
