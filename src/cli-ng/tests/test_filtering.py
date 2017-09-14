#!/usr/bin/python2
# -*- encoding: utf-8 -*-
import logging
try:
    import unittest2 as unittest
except ImportError:
    import unittest

import datetime

import clitests
import problem

from abrtcli.filtering import (filter_reported,
                               filter_not_reported,
                               filter_since,
                               filter_since_timestamp,
                               filter_until,
                               filter_until_timestamp)


class FilteringTestCase(clitests.TestCase):
    '''
    Test filtering functionality
    '''

    def test_filter_since(self):
        pl = problem.list()
        since = datetime.datetime(2015, 1, 1, 1, 1, 1)
        res = filter_since(pl, since)
        self.assertEqual(len(res), 3)

    def test_filter_since_timestamp(self):
        pl = problem.list()
        since = datetime.datetime(2015, 1, 1, 1, 1, 1)
        since_ts = since.strftime('%s')
        res = filter_since_timestamp(pl, since_ts)
        self.assertEqual(len(res), 3)

    def test_filter_until(self):
        pl = problem.list()
        until = datetime.datetime(2015, 1, 1, 1, 1, 1)
        res = filter_until(pl, until)
        self.assertEqual(len(res), 2)

    def test_filter_until_timestamp(self):
        pl = problem.list()
        until = datetime.datetime(2015, 1, 1, 1, 1, 1)
        until_ts = until.strftime('%s')
        res = filter_until_timestamp(pl, until_ts)
        self.assertEqual(len(res), 2)

    def test_filter_reported(self):
        pl = problem.list()
        res = filter_reported(pl)
        self.assertEqual(len(res), 1)

    def test_filter_not_reported(self):
        pl = problem.list()
        res = filter_not_reported(pl)
        self.assertEqual(len(res), 4)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    unittest.main()
