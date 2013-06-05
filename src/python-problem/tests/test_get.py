#!/usr/bin/env python
import os
import sys
import logging
import unittest

sys.path.insert(0, os.path.abspath(".."))
os.environ["PATH"] = "{0}:{1}".format(os.path.abspath(".."), os.environ["PATH"])

from nose import tools

from base import ProblematicTestCase

import problem

class GetTestCase(ProblematicTestCase):
    def test_get(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        prob2 = problem.get(ident, False, self.proxy)
        prob3 = problem.get(ident, True, self.proxy)

        tools.eq_(prob.reason, prob2.reason)
        tools.eq_(prob.reason, prob3.reason)

        prob.delete()

    def test_get_nonexistent(self):
        tools.ok_(problem.get('random', False, self.proxy) is None)

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
