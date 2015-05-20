import os
import logging
import report

import problem
import problem.config


class DBusProxy(object):
    __instance = None

    def __init__(self, dbus):
        self._proxy = None
        self._iface = None
        self.dbus = dbus
        self.connected = False
        self.connect()

    def __new__(cls, *args, **kwargs):
        if not cls.__instance:
            cls.__instance = super(DBusProxy, cls).__new__(cls)

        return cls.__instance

    def connect(self):
        self.connected = False
        if self._proxy:
            try:
                # we might get org.freedesktop.DBus.Error.ServiceUnknown here
                # if endpoint timed out
                self._proxy.close()
            except self.dbus.exceptions.DBusException:
                pass
        try:
            self._proxy = self.dbus.SystemBus().get_object(
                'org.freedesktop.problems', '/org/freedesktop/problems')
        except self.dbus.exceptions.DBusException as e:
            logging.debug('Unable to get dbus proxy: {0}'.format(e))
            return

        try:
            self._iface = self.dbus.Interface(self._proxy,
                                              'org.freedesktop.problems')
        except self.dbus.exceptions.DBusException as e:
            logging.debug('Unable to get dbus interface: {0}'.format(e))
            return

        self.connected = True

    def _dbus_call(self, fun_name, *args):
        try:
            logging.debug('Calling {0} with {1}'.format(fun_name, args))
            return getattr(self._iface, fun_name)(*args)
        except self.dbus.exceptions.DBusException as e:
            dbname = e.get_dbus_name()
            if dbname == "org.freedesktop.DBus.Error.ServiceUnknown":
                self.connect()
                return getattr(self._iface, fun_name)(*args)

            if dbname == 'org.freedesktop.problems.AuthFailure':
                raise problem.exception.AuthFailure(e)

            if dbname == 'org.freedesktop.problems.InvalidProblemDir':
                raise problem.exception.InvalidProblem(e)

            raise

    def get_item(self, dump_dir, name):
        val = self._dbus_call('GetInfo', dump_dir, [name])
        if name not in val:
            return None

        return str(val[name])

    def set_item(self, dump_dir, name, value):
        return self._dbus_call('SetElement', dump_dir, name, str(value))

    def del_item(self, dump_dir, name):
        return self._dbus_call('DeleteElement', dump_dir, name)

    def create(self, problem_dict):
        return self._dbus_call('NewProblem', problem_dict)

    def delete(self, dump_dir):
        return self._dbus_call('DeleteProblem', [dump_dir])

    def list(self):
        return [str(prob) for prob in self._dbus_call('GetProblems')]

    def list_all(self):
        return [str(prob) for prob in self._dbus_call('GetAllProblems')]


class SocketProxy(object):
    def create(self, problem_dict):
        import socket
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(5)
        try:
            sock.connect('/var/run/abrt/abrt.socket')
            sock.sendall("PUT / HTTP/1.1\r\n\r\n")
            for key, value in problem_dict.items():
                sock.sendall('{0}={1}\0'.format(key.upper(), value))

            sock.shutdown(socket.SHUT_WR)
            resp = ''
            while True:
                buf = sock.recv(256)
                if not buf:
                    break
                resp += buf
            return resp
        except socket.timeout as exc:
            logging.error('communication with daemon failed: {0}'.format(exc))
            return None

    def get_item(self, *args):
        raise NotImplementedError

    def set_item(self, *args):
        raise NotImplementedError

    def del_item(self, *args):
        raise NotImplementedError

    def delete(self, *args):
        raise NotImplementedError

    def list(self, *args):
        raise NotImplementedError

    def list_all(self, *args):
        return self.list(*args)

    def get_problem_watcher(self):
        raise NotImplementedError


class FsProxy(object):
    def __init__(self, directory=problem.config.DEFAULT_DUMP_LOCATION):
        self.directory = directory

    def create(self, problem_dict):
        probd = report.problem_data()
        for key, value in problem_dict.items():
            probd.add(key, value)

        ddir = probd.create_dump_dir(self.directory)
        ret = ddir.name
        ddir.close()
        problem.notify_new_path(ret)
        return ret

    def _open_ddir(self, dump_dir, readonly=False):
        flags = 0
        if readonly:
            flags |= report.DD_OPEN_READONLY

        ddir = report.dd_opendir(dump_dir, flags)
        if not ddir:
            raise problem.exception.InvalidProblem(
                'Can\'t open directory: {0}'.format(dump_dir))

        return ddir

    def get_item(self, dump_dir, name):
        ddir = self._open_ddir(dump_dir, readonly=True)

        flags = (report.DD_FAIL_QUIETLY_EACCES |
                 report.DD_FAIL_QUIETLY_ENOENT |
                 report.DD_LOAD_TEXT_RETURN_NULL_ON_FAILURE)

        val = ddir.load_text(name, flags).encode('utf-8', errors='ignore')

        ddir.close()
        return val

    def set_item(self, dump_dir, name, value):
        ddir = self._open_ddir(dump_dir)
        ddir.save_text(name, str(value))
        ddir.close()

    def del_item(self, dump_dir, name):
        ddir = self._open_ddir(dump_dir)
        ddir.delete_item(name)
        ddir.close()

    def delete(self, dump_dir):
        ddir = report.dd_opendir(dump_dir)
        if not ddir:
            return not os.path.isdir(dump_dir)

        ddir.delete()
        return True

    def list(self, _all=False):
        for dir_entry in os.listdir(self.directory):
            dump_dir = os.path.join(self.directory, dir_entry)

            if not os.path.isdir(dump_dir) or not os.access(dump_dir, os.R_OK):
                continue

            uid = os.getuid()
            gid = os.getuid()
            dir_stat = os.stat(dump_dir)
            if not _all and (dir_stat.st_uid != uid and
                             dir_stat.st_gid != gid):
                continue

            ddir = report.dd_opendir(dump_dir, report.DD_OPEN_READONLY)
            if ddir:
                ddir.close()
                yield dump_dir

    def list_all(self, *args, **kwargs):
        kwargs.update(dict(_all=True))
        return self.list(*args, **kwargs)


def get_proxy():
    try:
        import dbus
        wrapper = DBusProxy(dbus)
        if wrapper.connected:
            return wrapper
    except ImportError:
        logging.debug('DBus not found')

    return FsProxy()
