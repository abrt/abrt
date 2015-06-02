import problem
import datetime

# contains two problems with the same hash
# one problem of users app
# one unknown problem

_rt = '''uReport: BTHASH=3505a6db8a6bd51a3d690f1553b309a0d7eda948
ABRT Server: URL=https://retrace.fedoraproject.org/faf/reports/bthash/3505a6db8a6bd51a3d690f1553b309a0d7eda948
Bugzilla: URL=https://bugzilla.redhat.com/show_bug.cgi?id=1223349'''


_data = [
    {
        'type': problem.CCPP,
        'reason': 'pavucontrol killed by SIGSEGV',
        '_id': 'bc60a5cbddb4e3667511e718ceecac16133acc97',
        'path': '/var/tmp/abrt/ccpp-2015-05-16-14:41:47-7729',
        'count': 15,
        'time': datetime.datetime(2015, 5, 16, 14, 41, 47),
        'component': 'pavucontrol',
        'reported_to': _rt,
    },
    {
        'type': problem.CCPP,
        'reason': 'polkit killed by SIGSEGV',
        '_id': 'bc60a5cbddb4e3667511e718ceecac16133acc97',
        'path': '/var/tmp/abrt/ccpp-2015-06-16-14:41:47-7729',
        'count': 1,
        'time': datetime.datetime(2015, 6, 16, 14, 41, 47),
        'component': 'polkitd',
    },
    {
        'type': problem.CCPP,
        'reason': 'pavucontrol killed by SIGSEGV',
        '_id': 'acbea5cbddb4e3667511e718ceecac16133acc97',
        'path': '/var/tmp/abrt/ccpp-2015-06-16-14:41:47-7729',
        'count': 3,
        'time': datetime.datetime(2013, 6, 16, 14, 41, 47),
        'component': 'pavucontrol',
    },
    {
        'type': problem.CCPP,
        'reason': 'user_app killed by SIGSEGV',
        '_id': 'ffe635cbdd54e3667511e718ceecac16133acc97',
        'path': '/var/tmp/abrt/ccpp-2015-03-16-14:41:47-7729',
        'count': 1,
        'time': datetime.datetime(2015, 6, 17, 14, 41, 47),
        'executable': '/home/user/bin/user_app',
        'uid': 1234,
    },
    {
        'type': 'unknown_problem',
        'reason': 'something wrong happened',
        '_id': 'ccacca5cbdd54e3667511e718ceecac16133acc97',
        'path': '/var/tmp/abrt/ccpp-2014-03-16-14:41:47-7729',
        'count': 1,
        'time': datetime.datetime(2014, 6, 16, 14, 41, 47),
        'not-reportable': 'Not reportable reason',
    },
]


class FakeProxy():
    ''' To be sure no dbus api calls are made during tests '''
    def connect():
        pass

    def get_item(*args, **kwargs):
        return None


def get_fake_problems(*args, **kwargs):
    res = []
    for pdata in _data:
        p = problem.Problem(pdata['type'], pdata['reason'])

        for field in set(pdata.keys()) - set(['type', 'reason']):
            if field == 'path':  # path is immutable
                continue

            setattr(p, field, pdata[field])

        setattr(p, '_persisted', True)
        setattr(p, '_probdir',  pdata['path'])
        setattr(p, '_proxy',  FakeProxy())

        res.append(p)

    return res
