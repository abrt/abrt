#!/usr/bin/env python
import os
import sys
import logging
import unittest

sys.path.insert(0, os.path.abspath(".."))
sys.path.insert(0, os.path.abspath("../problem/.libs"))  # because of _pyabrt
os.environ["PATH"] = "{0}:{1}".format(os.path.abspath(".."), os.environ["PATH"])

from nose import tools

from base import ProblematicTestCase


class PropertiesTestCase(ProblematicTestCase):
    def test_path(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        tools.eq_(prob.path, None)

        prob._probdir = '/tmp/test'
        prob.save()

        tools.eq_(prob.path, prob._probdir)

    def test_ids(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        tools.eq_(prob.id, None)
        tools.eq_(prob.short_id, None)

        prob.save()
        # fix probdir to test value
        prob._probdir = '/tmp/test'

        hid = 'f78bf4900bc160fcc5d4e67ae53e392b2775b190'
        tools.eq_(prob.id, hid)
        tools.eq_(prob.short_id, hid[:7])

    def test_not_reportable_sets_empty_reason(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        prob.not_reportable = True

        tools.eq_(prob.not_reportable, True)
        tools.eq_(prob.not_reportable_reason, '')

    def test_not_reportable_with_reason(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        prob.not_reportable = True
        prob.not_reportable_reason = 'dunno'

        tools.eq_(prob.not_reportable, True)
        tools.eq_(prob.not_reportable_reason, 'dunno')

    def test_not_reportable_reset(self):
        prob = self.create_problem()
        prob.add_current_process_data()

        prob.not_reportable = True
        prob.not_reportable = False

        tools.eq_(prob.not_reportable, False)
        tools.eq_(prob.not_reportable_reason, None)


if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG)
    unittest.main()
