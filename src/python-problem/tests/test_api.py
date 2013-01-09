#!/usr/bin/env python
import os
import sys
import time
import logging
import datetime
import unittest

sys.path.insert(0, os.path.abspath(".."))
os.environ["PATH"] = "{0}:{1}".format(os.path.abspath(".."), os.environ["PATH"])

from nose import tools

from base import ProblematicTestCase

import problem

class ProblemAPITestCase(ProblematicTestCase):
    def test_init(self):
        prob = self.create_problem()

        tools.eq_(prob.type, problem.RUNTIME)
        tools.eq_(prob.analyzer, problem.RUNTIME)
        tools.eq_(prob.reason, 'Front fell off')

    def test_add_current_process_data(self):
        prob = self.create_problem()

        prob.add_current_process_data()
        tools.eq_(prob.pid, os.getpid())
        tools.eq_(prob.gid, os.getgid())
        tools.ok_(
            '<stdin>'  in prob.executable or
            'tests.py' in prob.executable or
            'test_api.py' in prob.executable or
            'nosetest' in prob.executable)

    def test_getattr(self):
        prob = self.create_problem()

        tools.eq_(prob.reason, 'Front fell off')

        with self.assertRaises(AttributeError):
            prob.non_existent

        with self.assertRaises(AttributeError):
            prob.non_existent_method()

        prob.add_current_process_data()
        ident = prob.save()

        self.proxy.set_item(ident, 'test', 'wat')
        tools.eq_(prob.test, 'wat')

        with self.assertRaises(AttributeError):
            prob.persisted_non_existent

        with self.assertRaises(AttributeError):
            prob.persisted_non_existent_method()

        prob.delete()

    def test_getattr_on_deleted(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        prob.save()
        del prob.executable

        with self.assertRaises(AttributeError):
            prob.executable

        prob.delete()

    def test_getattr_on_persisted(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        ident = prob.save()

        self.proxy.set_item(ident, 'test', 1)

        tools.eq_(prob.test, 1)

        prob.delete()

    def test_getitem(self):
        prob = self.create_problem()

        tools.eq_(prob['reason'], 'Front fell off')

        with self.assertRaises(KeyError):
            prob['non_existent']

        prob.add_current_process_data()
        ident = prob.save()

        self.proxy.set_item(ident, 'test', 'wat')
        tools.eq_(prob['test'], 'wat')

        with self.assertRaises(KeyError):
            prob['persisted_non_existent']

        prob.delete()

    def test_setattr(self):
        prob = self.create_problem()

        prob.test = 'x'
        tools.eq_(prob.test, 'x')

        prob._test = 'y'
        tools.eq_(prob._test, 'y')

        prob.add_current_process_data()
        ident = prob.save()

        tools.eq_(self.proxy.get_item(ident, 'test'), 'x')
        tools.eq_(self.proxy.get_item(ident, '_test'), None)

        prob.delete()

    def test_setattr_on_persisted(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        ident = prob.save()

        self.proxy.set_item(ident, 'test', 'wat')

        prob.test = '14'
        prob.save()

        tools.eq_(self.proxy.get_item(ident, 'test'), '14')

        prob.delete()


    def test_setitem(self):
        prob = self.create_problem()

        prob['test'] = 'x'
        tools.eq_(prob.test, 'x')
        tools.eq_(prob['test'], 'x')

        prob['_test'] = 'y'

        prob.add_current_process_data()
        ident = prob.save()

        tools.eq_(self.proxy.get_item(ident, 'test'), 'x')
        tools.eq_(self.proxy.get_item(ident, '_test'), None)

        prob.delete()

    def test_delattr(self):
        prob = self.create_problem()
        del prob.reason
        with self.assertRaises(AttributeError):
            prob.reason

        with self.assertRaises(AttributeError):
            del prob.non_existent

        prob.add_current_process_data()
        ident = prob.save()

        tools.eq_(self.proxy.get_item(ident, 'reason'), None)

        del prob.type
        prob.save()

        tools.eq_(self.proxy.get_item(ident, 'type'), None)

        prob.delete()

    def test_delattr_on_persisted(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        ident = prob.save()

        self.proxy.set_item(ident, 'test', 'wat')

        del prob.test

        with self.assertRaises(AttributeError):
            del prob.test

        prob.save()

        tools.eq_(self.proxy.get_item(ident, 'test'), None)

        prob.delete()

    def test_delitem(self):
        prob = self.create_problem()

        del prob['reason']
        with self.assertRaises(KeyError):
            prob['reason']

        with self.assertRaises(KeyError):
            del prob['non_existent']

        prob.add_current_process_data()
        ident = prob.save()

        tools.eq_(self.proxy.get_item(ident, 'reason'), None)

        del prob['type']
        prob.save()

        tools.eq_(self.proxy.get_item(ident, 'type'), None)

        prob.delete()

    def test_int_cast(self):
        prob = self.create_problem()

        prob.add_current_process_data()
        prob['mynumerical'] = 15
        ident = prob.save()

        tools.eq_(self.proxy.get_item(ident, 'mynumerical'), '15')
        self.proxy.set_item(ident, 'numerical', '123')

        tools.eq_(prob.numerical, 123)

        prob.delete()

    def test_time_cast(self):

        if type(self.proxy) == problem.proxies.DBusProxy:
            # set_item time is not allowed by the daemon
            return unittest.skip('Skipping time cast test on DBusProxy')

        prob = self.create_problem()

        prob.add_current_process_data()
        ident = prob.save()

        saved_time = int(time.time())
        self.proxy.set_item(ident, 'time', str(saved_time))
        cast_time = prob.time

        tools.eq_(cast_time, datetime.datetime.fromtimestamp(int(saved_time)))
        tools.eq_(type(cast_time), datetime.datetime)

        prob.time += datetime.timedelta(days=3)
        prob.save()

        updated_time = self.proxy.get_item(ident, 'time')

        tools.ok_(type(updated_time), str)
        tools.ok_(updated_time != saved_time)

        prob.delete()

    def test_add_current_environment(self):
        prob = self.create_problem()

        prob.add_current_environment()

        for key, value in os.environ.iteritems():
            tools.ok_('{0}={1}'.format(key, value) in prob.environ)

    def test_save_delete(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        path = prob.save()
        tools.ok_('runtime-' in path)

        prob.delete()

    def test_repr(self):
        prob = self.create_problem()
        ret = repr(prob)
        tools.ok_('problem.Runtime' in ret)
        tools.ok_('(Front fell off)' in ret)

    def test_items(self):
        prob = self.create_problem()
        for key, value in prob.items():
            tools.eq_(prob[key], value)

    def test_validate(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        prob.validate()

        del prob.executable
        with self.assertRaises(problem.exception.ValidationError):
            prob.validate()

    def test_invalidproblem(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()
        prob.delete()

        time.sleep(2)

        with self.assertRaises(problem.exception.InvalidProblem):
            self.proxy.get_item(ident, 'reason')

    def test_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        tools.eq_(self.proxy.get_item(ident, 'type'), problem.RUNTIME)
        tools.eq_(self.proxy.get_item(ident, 'analyzer'), problem.RUNTIME)
        tools.eq_(self.proxy.get_item(ident, 'reason'), 'Front fell off')
        tools.ok_(self.proxy.get_item(ident, 'pid') is not None)
        tools.ok_(self.proxy.get_item(ident, 'gid') is not None)
        tools.ok_(self.proxy.get_item(ident, 'executable') is not None)

        prob.delete()

    def test_dirty_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        prob.executable = 'nine'
        prob.save()

        tools.eq_(self.proxy.get_item(ident, 'executable'), 'nine')

        prob.delete()

    def test_delete(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        tools.ok_(ident in self.proxy.list())

        prob.delete()

        tools.ok_(ident not in self.proxy.list())

    def test_delete_then_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()
        prob.delete()
        ident2 = prob.save()

        tools.ok_(ident != ident2)

        prob.delete()

    def test_problem_types(self):
        for ptype, internal in problem.PROBLEM_TYPES.items():
            class_name = ptype.lower().capitalize()
            prinstance = getattr(problem, class_name)('Front fell off')
            tools.eq_(prinstance.type, internal)
            tools.eq_(prinstance.analyzer, internal)

        unpr = problem.Unknown('Front not found')
        tools.eq_(unpr.type, 'libreport')

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
