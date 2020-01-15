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

class GetTestCase(ProblematicTestCase):
    def test_get(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        prob2 = problem.get(ident, False, self.proxy)
        prob3 = problem.get(ident, True, self.proxy)

        assert prob.reason == prob2.reason
        assert prob.reason == prob3.reason

        prob.delete()

    def test_get_nonexistent(self):
        assert problem.get('random', False, self.proxy) is None

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
