#!/usr/bin/env python3
import os
import sys
import time
import logging
import datetime
import unittest

sys.path.insert(0, os.path.abspath(".."))
sys.path.insert(0, os.path.abspath("../problem/.libs"))  # because of _pyabrt
os.environ["PATH"] = "{0}:{1}".format(os.path.abspath(".."), os.environ["PATH"])

from base import ProblematicTestCase

import problem

class ProblemAPITestCase(ProblematicTestCase):
    def test_init(self):
        prob = self.create_problem()

        assert prob.type == problem.RUNTIME
        assert prob.analyzer == problem.RUNTIME
        assert prob.reason == 'Front fell off'

    def test_add_current_process_data(self):
        prob = self.create_problem()

        prob.add_current_process_data()
        assert prob.pid == os.getpid()
        assert prob.gid == os.getgid()
        assert (
            '<stdin>'  in prob.executable or
            'tests.py' in prob.executable or
            'test_api.py' in prob.executable or
            'pytest' in prob.executable)

    def test_getattr(self):
        prob = self.create_problem()

        assert prob.reason == 'Front fell off'

        self.assertRaises(AttributeError, lambda: prob.non_existent)

        prob.add_current_process_data()
        ident = prob.save()

        self.proxy.set_item(ident, 'test', 'wat')
        assert prob.test == 'wat'

        self.assertRaises(AttributeError, lambda: prob.persisted_non_existent)

        prob.delete()

    def test_getattr_on_deleted(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        prob.save()
        del prob.executable

        self.assertRaises(AttributeError, getattr, prob, 'executable')

        prob.delete()

    def test_getattr_on_persisted(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        ident = prob.save()

        self.proxy.set_item(ident, 'test', 1)

        assert prob.test == 1

        prob.delete()

    def test_getitem(self):
        prob = self.create_problem()

        assert prob['reason'] == 'Front fell off'

        self.assertRaises(KeyError, lambda:  prob['non_existent'])

        prob.add_current_process_data()
        ident = prob.save()

        self.proxy.set_item(ident, 'test', 'wat')
        assert prob['test'] == 'wat'

        self.assertRaises(KeyError, lambda: prob['persisted_non_existent'])

        prob.delete()

    def test_setattr(self):
        prob = self.create_problem()

        prob.test = 'x'
        assert prob.test == 'x'

        prob._test = 'y'
        assert prob._test == 'y'

        prob.add_current_process_data()
        ident = prob.save()

        assert self.proxy.get_item(ident, 'test') == 'x'
        assert self.proxy.get_item(ident, '_test') == None

        prob.delete()

    def test_setattr_on_persisted(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        ident = prob.save()

        self.proxy.set_item(ident, 'test', 'wat')

        prob.test = '14'
        prob.save()

        assert self.proxy.get_item(ident, 'test') == '14'

        prob.delete()


    def test_setitem(self):
        prob = self.create_problem()

        prob['test'] = 'x'
        assert prob.test == 'x'
        assert prob['test'] == 'x'

        prob['_test'] = 'y'

        prob.add_current_process_data()
        ident = prob.save()

        assert self.proxy.get_item(ident, 'test') == 'x'
        assert self.proxy.get_item(ident, '_test') == None

        prob.delete()

    def test_delattr(self):
        prob = self.create_problem()
        del prob.reason
        self.assertRaises(AttributeError, lambda: prob.reason)

        self.assertRaises(AttributeError, lambda: prob.non_existant)

        prob.add_current_process_data()
        ident = prob.save()

        assert self.proxy.get_item(ident, 'reason') == None

        del prob.type
        prob.save()

        assert self.proxy.get_item(ident, 'type') == None

        prob.delete()

    def test_delattr_on_persisted(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        ident = prob.save()

        self.proxy.set_item(ident, 'test', 'wat')

        del prob.test

        def raising_delete():
            del prob.test

        self.assertRaises(AttributeError, raising_delete)

        prob.save()

        assert self.proxy.get_item(ident, 'test') == None

        prob.delete()

    def test_delitem(self):
        prob = self.create_problem()

        del prob['reason']
        self.assertRaises(KeyError, lambda: prob['reason'])

        def raising_delete():
            del prob['non_existent']

        self.assertRaises(KeyError, raising_delete)

        prob.add_current_process_data()
        ident = prob.save()

        assert self.proxy.get_item(ident, 'reason') == None

        del prob['type']
        prob.save()

        assert self.proxy.get_item(ident, 'type') == None

        prob.delete()

    def test_int_cast(self):
        prob = self.create_problem()

        prob.add_current_process_data()
        prob['mynumerical'] = 15
        ident = prob.save()

        assert self.proxy.get_item(ident, 'mynumerical') == '15'
        self.proxy.set_item(ident, 'numerical', '123')

        assert prob.numerical == 123

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

        assert cast_time == datetime.datetime.fromtimestamp(int(saved_time))
        assert type(cast_time) == datetime.datetime

        prob.time += datetime.timedelta(days=3)
        prob.save()

        updated_time = self.proxy.get_item(ident, 'time')

        assert type(updated_time) == str
        assert updated_time != saved_time

        prob.delete()

    def test_add_current_environment(self):
        prob = self.create_problem()

        prob.add_current_environment()

        for key, value in os.environ.items():
            assert '{0}={1}'.format(key, value) in prob.environ

    def test_save_delete(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        path = prob.save()
        assert 'runtime-' in path

        prob.delete()

    def test_repr(self):
        prob = self.create_problem()
        ret = repr(prob)
        assert 'problem.Runtime' in ret
        assert '(Front fell off)' in ret

    def test_items(self):
        prob = self.create_problem()
        for key, value in prob.items():
            assert prob[key] == value

    def test_validate(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        prob.validate()

        del prob.executable
        self.assertRaises(problem.exception.ValidationError, prob.validate)

    def test_invalidproblem(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()
        prob.delete()

        time.sleep(2)

        self.assertRaises(problem.exception.InvalidProblem, self.proxy.get_item,ident, 'reason')

    def test_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        assert self.proxy.get_item(ident, 'type') == problem.RUNTIME
        assert self.proxy.get_item(ident, 'analyzer') == problem.RUNTIME
        assert self.proxy.get_item(ident, 'reason') == 'Front fell off'
        assert self.proxy.get_item(ident, 'pid') is not None
        assert self.proxy.get_item(ident, 'gid') is not None
        assert self.proxy.get_item(ident, 'executable') is not None

        prob.delete()

    def test_dirty_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        prob.executable = 'nine'
        prob.save()

        assert self.proxy.get_item(ident, 'executable') == 'nine'

        prob.delete()

    def test_delete(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        assert ident in self.proxy.list()

        prob.delete()

        assert ident not in self.proxy.list()

    def test_delete_then_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()
        prob.delete()
        ident2 = prob.save()

        assert ident != ident2

        prob.delete()

    def test_problem_types(self):
        for ptype, internal in problem.PROBLEM_TYPES.items():
            class_name = ptype.lower().capitalize()
            prinstance = getattr(problem, class_name)('Front fell off')
            assert prinstance.type == internal
            assert prinstance.analyzer == internal

        unpr = problem.Unknown('Front not found')
        assert unpr.type == 'libreport'

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
