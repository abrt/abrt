import os
import inspect
import datetime

from problem import proxies, exception, tools, watch
try:
    from _pyabrt import *
except ImportError:
    try:
        from _py3abrt import *
    except ImportError:
        from problem._py3abrt import *

JAVA = 'java'
SELINUX = 'selinux'
CCPP = 'CCpp'
PYTHON = 'Python'
KERNELOOPS = 'Kerneloops'
RUNTIME = 'runtime'
XORG = 'xorg'
UNKNOWN = 'libreport'

REQUIRED_FIELDS = ['executable']
PREFETCH_FIELDS = [
    # core fields
    'component', 'hostname', 'os_release', 'uid',
    'username', 'architecture', 'kernel', 'package',
    'time', 'count', 'pkg_arch', 'pkg_name',
    'pkg_epoch', 'pkg_version', 'pkg_release',
    'uuid',
    # type specific
    'cgroup', 'core_backtrace', 'backtrace',
    'dso_list', 'exploitable', 'maps',
    'cmdline', 'environ', 'open_fds', 'pid',
    'proc_pid_status', 'limits', 'var_log_messages',
    'suspend_stats', 'reported_to', 'event_log',
    'dmesg',
]

PROBLEM_TYPES = {
    'JAVA': JAVA,
    'SELINUX': SELINUX,
    'CCPP': CCPP,
    'PYTHON': PYTHON,
    'KERNELOOPS': KERNELOOPS,
    'RUNTIME': RUNTIME,
    'XORG': XORG,
    'UNKNOWN': UNKNOWN,
}


class Problem(object):
    '''
    Base class for the other problem types.

    No need to use this class directly, use one
    of the specific problem classes.

    '''
    def __init__(self, typ, reason, analyzer=None):
        self._data = dict()
        self._dirty_data = dict()
        self._persisted = False
        self._proxy = None
        self._probdir = None

        self.type = typ
        if analyzer is None:
            self.analyzer = typ
        self.reason = reason
        self._proxy = proxies.get_proxy()

    def __cast(self, attr, val, reverse=False):
        # str with digits -> int
        if not reverse and type(val) == str and val.isdigit():
            val = int(val)

        # by attr name
        mapping = {
            'time': (datetime.datetime.fromtimestamp,
                     lambda x: x.strftime('%s'))
        }

        if attr in mapping:
            fun, revfun = mapping[attr]
            if reverse:
                fun = revfun

            val = fun(val)

        if reverse:
            return str(val)

        return val

    def __getattr__(self, attr):
        exc = AttributeError("object has no attribute '{0}'".format(attr))
        val = None

        # was deleted before?
        if attr in self._dirty_data and self._dirty_data[attr] is None:
            raise exc

        if attr in self._data:
            val = self._data[attr]

        # try to fetch the item
        if self._persisted:
            val = self._proxy.get_item(self._probdir, attr)
            self._data[attr] = val

        if val is None:
            raise exc

        val = self.__cast(attr, val)
        super(Problem, self).__setattr__(attr, val)

        return val

    def __setattr__(self, attr, value):
        super(Problem, self).__setattr__(attr, value)
        if not attr[0] == '_':
            self._data[attr] = value
            if self._persisted:
                self._dirty_data[attr] = value

    def __delattr__(self, attr):
        # it might not be loaded at first
        self.__getattr__(attr)
        super(Problem, self).__delattr__(attr)
        del self._data[attr]
        if self._persisted:
            self._dirty_data[attr] = None

    def __getitem__(self, attr):
        try:
            return self.__getattr__(attr)
        except AttributeError as e:
            raise KeyError(e)

    def __setitem__(self, attr, value):
        self.__setattr__(attr, value)

    def __delitem__(self, attr):
        try:
            self.__delattr__(attr)
        except AttributeError as e:
            raise KeyError(e)

    def __repr__(self):
        return '<problem.{0} ({1})>'.format(self.__class__.__name__, self.reason)

    def add_current_process_data(self):
        ''' Add pid, gid and executable of current
        process to this problem object

        '''
        self.pid = os.getpid()
        self.gid = os.getgid()
        #self.executable = os.readlink('/proc/{0}/exe'.format(os.getpid()))
        # ^ always '/usr/bin/python' so we need:
        self.executable = os.path.abspath(inspect.stack()[-1][1])

    def add_current_environment(self):
        ''' Add environment of current process to this problem object '''
        self.environ = ''
        for key, value in os.environ.items():
            self.environ += '{0}={1}\n'.format(key, value)

    def prefetch_data(self):
        ''' Prefetch possible data fields of this problem '''
        if not self._persisted:
            return

        for field in PREFETCH_FIELDS:
            try:
                self.__getattr__(field)
            except AttributeError:
                pass

    def items(self):
        return self._data.items()

    def validate(self):
        for field in REQUIRED_FIELDS:
            if not hasattr(self, field):
                raise exception.ValidationError(
                    'Missing required field {0}'.format(field))

    def save(self):
        ''' Create this problem or update modified data

        Create or update the project if some of its fields
        were modified.

        Return ``None`` in case of modification, identifier
        if new problem was created.

        '''
        self.validate()

        # convert to strings
        str_data = dict()
        for key, value in self._data.items():
            str_data[str(key)] = self.__cast(key, value, reverse=True)

        # already persisted?
        if self._persisted:
            for key, value in self._dirty_data.items():
                if value is None:
                    self._proxy.del_item(self._probdir, key)
                else:
                    self._proxy.set_item(self._probdir, key,
                                         self.__cast(key, value, reverse=True))

            self._dirty_data = dict()
        else:
            # create
            ret = self._proxy.create(str_data)
            self._persisted = True
            self._probdir = str(ret)
            return self._probdir

    def delete(self):
        ''' Delete this problem '''
        if self._persisted:
            self._proxy.delete(self._probdir)
            self._persisted = False
            self._probdir = None
            self._dirty_data = {}


class Java(Problem):
    ''' Java problem '''
    def __init__(self, reason):
        super(Java, self).__init__(JAVA, reason)


class Selinux(Problem):
    ''' Selinux problem '''
    def __init__(self, reason):
        super(Selinux, self).__init__(SELINUX, reason)


class Ccpp(Problem):
    ''' C, C++ problem '''
    def __init__(self, reason):
        super(Ccpp, self).__init__(CCPP, reason)


class Python(Problem):
    ''' Python problem '''
    def __init__(self, reason):
        super(Python, self).__init__(PYTHON, reason)


class Kerneloops(Problem):
    ''' Kerneloops problem '''
    def __init__(self, reason):
        super(Kerneloops, self).__init__(KERNELOOPS, reason)


class Xorg(Problem):
    ''' Xorg problem '''
    def __init__(self, reason):
        super(Xorg, self).__init__(XORG, reason)


class Runtime(Problem):
    ''' Runtime problem '''
    def __init__(self, reason):
        super(Runtime, self).__init__(RUNTIME, reason)


class Unknown(Problem):
    ''' Unknown problem '''
    def __init__(self, reason):
        super(Unknown, self).__init__('libreport', reason)


def list(auth=False, __proxy=proxies.get_proxy()):
    ''' Return the list of the problems

    Use ``auth=True`` if authentication should be attempted.

    If authentication via polkit fails, function behaves
    as if ``auth=False`` was specified (only users problems are
    returned).
    '''
    fun = __proxy.list
    if auth:
        fun = __proxy.list_all

    return [tools.problemify(prob, __proxy) for prob in fun()]


def get(identifier, auth=False, __proxy=proxies.get_proxy()):
    ''' Return problem object matching ``identifier``

    Return ``None`` in case the problem does not exist.
    Use ``auth=True`` if authentication should be attempted.

    '''

    fun = __proxy.list
    if auth:
        fun = __proxy.list_all

    if identifier not in fun():
        return None

    return tools.problemify(identifier, __proxy)


def get_problem_watcher(auth=False):
    ''' Return ``ProblemWatcher`` object which can be used
    to attach callbacks called when new problem is created

    Use ``auth=True`` if authentication should be attempted for
    new problem that doesn't belong to current user. If not
    set such a problem is ignored.

    '''

    return watch.ProblemWatcher(auth)
