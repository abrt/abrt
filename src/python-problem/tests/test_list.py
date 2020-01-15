#!/usr/bin/env python3
import os
import sys
import logging

sys.path.insert(0, os.path.abspath(".."))
sys.path.insert(0, os.path.abspath("../problem/.libs"))  # because of _pyabrt
os.environ["PATH"] = "{0}:{1}".format(os.path.abspath(".."), os.environ["PATH"])

import unittest

from base import ProblematicTestCase

import problem

class ListTestCase(ProblematicTestCase):
    def test_list(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        assert ident in map(lambda x: x._probdir, problem.list(False, self.proxy))

        prob.delete()

    def test_list_all(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        assert ident in map(lambda x: x._probdir, problem.list(True, self.proxy))

        prob.delete()

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
