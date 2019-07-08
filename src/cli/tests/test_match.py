#!/usr/bin/python3
# -*- encoding: utf-8 -*-
import logging
try:
    import unittest2 as unittest
except ImportError:
    import unittest

import clitests

from abrtcli.match import (get_match_data,
                           match_completer,
                           match_get_problems,
                           match_lookup)

from abrtcli.utils import captured_output


class MatchTestCase(clitests.TestCase):
    '''
    Simple test to check if database creation & access
    works as expected.
    '''

    hashes = ['ccacca5', 'bc60a5c', 'acbea5c', 'ffe635c']
    collision_hash = 'bc60a5c'
    human = ['/home/user/bin/user_app', 'unknown_problem', 'polkitd']
    collision_human = 'pavucontrol'
    combined = ['pavucontrol@bc60a5c', 'pavucontrol@acbea5c']
    paths = [
        '/var/tmp/abrt/ccpp-2015-03-16-14:41:47-7729',
        '/var/tmp/abrt/ccpp-2015-06-16-14:41:47-7729',
        '/var/tmp/abrt/ccpp-2015-05-16-14:41:47-7729',
        '/var/tmp/abrt/ccpp-2014-03-16-14:41:47-7729',
    ]

    def test_get_match_data(self):
        '''
        Test get_match_data returns correctly merged data
        '''

        by_human_id, by_short_id, by_path = get_match_data()

        self.assertEqual(len(by_human_id), 4)
        self.assertEqual(len(by_short_id), 4)
        self.assertEqual(len(by_path), 4)

    def test_match_completer(self):
        '''
        Test that match_completer yields properly formatted candidates
        '''

        pm = match_completer(None, None)
        self.assertEqual(set(pm), set(self.hashes + self.human + self.combined + self.paths))

    def test_match_lookup_hash(self):
        '''
        Test match lookup by hash
        '''

        for h in self.hashes:
            m = match_lookup(h)
            self.assertGreaterEqual(len(m), 1)

    def test_match_lookup_human_id(self):
        '''
        Test match lookup by human id
        '''

        for h in self.human:
            m = match_lookup(h)
            self.assertEqual(len(m), 1)

    def test_match_lookup_combined(self):
        '''
        Test match lookup by human id combined with short id
        '''

        for h in self.combined:
            m = match_lookup(h)
            self.assertEqual(len(m), 1)

    def test_match_lookup_collisions(self):
        '''
        Test match lookup handles collisions
        '''

        m = match_lookup(self.collision_human)
        self.assertEqual(len(m), 2)

    def test_match_lookup_nonexistent(self):
        '''
        Test match lookup handles empty input
        '''

        m = match_lookup('')
        self.assertEqual(len(m), 1)
        self.assertEqual(m[0].id, 'ffe635cbdd54e3667511e718ceecac16133acc97')

    def test_match_get_problem_simple(self):
        '''
        Test that match_get_problem matches unique pattern
        '''

        p = match_get_problems('polkitd')
        self.assertEqual(len(p), 1)
        self.assertEqual(p[0].component, 'polkitd')

    def test_match_get_problem_multiple(self):
        '''
        Test that match_get_problem returns all matching problems
        '''

        p = match_get_problems('pavucontrol')
        self.assertEqual(len(p), 2)
        self.assertEqual("%s@%s" % (p[0].component, p[0].short_id), "pavucontrol@bc60a5c")
        self.assertEqual("%s@%s" % (p[1].component, p[1].short_id), "pavucontrol@acbea5c")

    def test_match_get_problem_empty_database(self):
        '''
        Test that match_get_problem handles no problems in database
        '''

        import problem

        with clitests.monkey_patch(problem, 'list', lambda *args, **kwargs: []):
            with captured_output() as (cap_stdout, cap_stderr):
                with self.assertRaises(SystemExit):
                    match_get_problems('nope')

            stdout = cap_stdout.getvalue()
            self.assertIn("No matching problems found", stdout)

            # Similar with last
            with captured_output() as (cap_stdout, cap_stderr):
                with self.assertRaises(SystemExit):
                    match_get_problems('last')

            stdout = cap_stdout.getvalue()
            self.assertIn("No matching problems found", stdout)

    def test_match_get_problem_nonexistent(self):
        '''
        Test that match_get_problem exits on non-existent problem
        '''

        with self.assertRaises(SystemExit):
            match_get_problems('nope')


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    unittest.main()
