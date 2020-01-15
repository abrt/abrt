#!/usr/bin/env python3
import os
import sys
import logging
import unittest

sys.path.insert(0, os.path.abspath(".."))
sys.path.insert(0, os.path.abspath("../problem/.libs"))  # because of _pyabrt
os.environ["PATH"] = "{0}:{1}".format(os.path.abspath(".."), os.environ["PATH"])

from base import ProblematicTestCase

import problem

class InternalProblemImplementationTestCase(ProblematicTestCase):
    def test_init(self):
        prob = self.create_problem()
        assert prob._proxy == self.proxy

    def test_setattr(self):
        prob = self.create_problem()

        prob.test = 0
        assert prob._data['test'] == 0
        assert prob._dirty_data == {}

        prob._test = 1
        assert prob._test == 1
        assert '_test' not in prob._data

        prob.add_current_process_data()
        prob.save()

        prob.persisted_test = 0
        assert prob._data['persisted_test'] == 0
        assert prob._dirty_data['persisted_test'] == 0

        prob.delete()

    def test_setitem(self):
        prob = self.create_problem()

        prob['test'] = 0
        assert prob._data['test'] == 0
        assert prob._dirty_data == {}

        prob['_test'] = 1
        assert '_test' not in prob._data

        prob.add_current_process_data()
        prob.save()

        prob['persisted_test'] = 0
        assert prob._data['persisted_test'] == 0
        assert prob._dirty_data['persisted_test'] == 0

        prob.delete()

    def test_delattr(self):
        prob = self.create_problem()
        del prob.reason
        assert 'reason' not in prob._data

        prob.add_current_process_data()
        prob.save()

        del prob.type
        assert prob._dirty_data == {'type': None}

        prob.save()

        assert prob._dirty_data == {}

        prob.delete()

    def test_delitem(self):
        prob = self.create_problem()
        del prob['reason']
        assert 'reason' not in prob._data

        prob.add_current_process_data()
        prob.save()

        del prob['type']
        assert prob._dirty_data == {'type': None}

        prob.save()

        assert prob._dirty_data == {}

        prob.delete()

    def test_items(self):
        prob = problem.Runtime('Massive error')
        assert prob.items() == prob._data.items()

    def test_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        assert prob._probdir == ident

        prob.delete()

    def test_dirty_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        prob.executable = 'nine'

        assert prob._dirty_data['executable'] == 'nine'
        prob.save()
        assert prob._dirty_data == {}

        prob.delete()

    def test_delete(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        prob.delete()

        assert not prob._persisted
        assert prob._probdir == None
        assert prob._dirty_data == {}

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
