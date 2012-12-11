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

class ProblemifyTestCase(ProblematicTestCase):
    def test_problemify(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        prob2 = problem.tools.problemify(ident, self.proxy)

        tools.eq_(type(prob), type(prob2))
        tools.eq_(prob.type, prob2.type)
        tools.eq_(prob.analyzer, prob2.analyzer)
        tools.eq_(prob.reason, prob2.reason)
        tools.eq_(prob.executable, prob2.executable)

        prob.delete()

    def test_problemify_unknown(self):
        prob = problem.Unknown('Front not found')
        prob._proxy = self.proxy
        prob.add_current_process_data()
        ident = prob.save()

        prob2 = problem.tools.problemify(ident, self.proxy)
        tools.eq_(type(prob2), problem.Unknown)

        prob.delete()

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
