#!/usr/bin/python3
# -*- encoding: utf-8 -*-
import logging
import unittest

import datetime

import problem

from abrtcli.filtering import (filter_components,
                               filter_executables,
                               filter_ids,
                               filter_paths,
                               filter_reported,
                               filter_not_reported,
                               filter_since,
                               filter_since_timestamp,
                               filter_until,
                               filter_until_timestamp)
import clitests


class FilteringTestCase(clitests.TestCase):
    '''
    Test filtering functionality
    '''

    def test_filter_components(self):
        problems = problem.list()
        components = ['pavucontrol']
        problems = filter_components(problems, components)
        self.assertEqual(len(problems), 2)

    def test_filter_executables(self):
        problems = problem.list()
        executables = ['/home/user/bin/user_app']
        problems = filter_executables(problems, executables)
        self.assertEqual(len(problems), 1)

    def test_filter_ids(self):
        problems = problem.list()
        self.assertListEqual(problems, filter_ids(problems, []))
        ids = ['bc60a5cbddb4e3667511e718ceecac16133acc97', 'ccacca5']
        problems = filter_ids(problems, ids)
        # There’s an ID collision.
        self.assertEqual(len(problems), 3)

    def test_filter_paths(self):
        problems = problem.list()
        self.assertListEqual(problems, filter_paths(problems, []))
        paths = ['/var/tmp/abrt/ccpp-2014-03-16-14:41:47-7729',
                 '/var/tmp/abrt/ccpp-2015-03-16-14:41:47-7729',
                 '/var/tmp/abrt/ccpp-2015-05-16-14:41:47-7729']
        problems = filter_paths(problems, paths)
        self.assertEqual(len(problems), 3)

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
