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
                           match_get_problem,
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

    def test_get_match_data(self):
        '''
        Test get_match_data returns correctly merged data
        '''

        by_human_id, by_short_id = get_match_data()
        self.assertEqual(len(by_human_id), 4)

        self.assertEqual(len(by_short_id), 4)

    def test_match_completer(self):
        '''
        Test that match_completer yields properly formatted candidates
        '''

        pm = match_completer(None, None)
        self.assertEqual(set(pm), set(self.hashes + self.human + self.combined))

    def test_match_lookup_hash(self):
        '''
        Test match lookup by hash
        '''

        for h in self.hashes:
            m = match_lookup(h)
            self.assertTrue(len(m) >= 1)

    def test_match_lookup_human_id(self):
        '''
        Test match lookup by human id
        '''

        for h in self.human:
            m = match_lookup(h)
            self.assertTrue(len(m) == 1)

    def test_match_lookup_combined(self):
        '''
        Test match lookup by human id
        '''

        for h in self.combined:
            m = match_lookup(h)
            self.assertTrue(len(m) == 1)

    def test_match_lookup_collisions(self):
        '''
        Test match lookup handles collisions
        '''

        m = match_lookup(self.collision_hash)
        self.assertTrue(len(m) == 2)

        m = match_lookup(self.collision_human)
        self.assertTrue(len(m) == 2)

    def test_match_lookup_nonexistent(self):
        '''
        Test match lookup handles empty input
        '''

        m = match_lookup('')
        self.assertEqual(m, None)

    def test_match_get_problem_simple(self):
        '''
        Test that match_get_problem matches unique pattern
        '''

        p = match_get_problem('polkitd')
        self.assertEqual(p.component, 'polkitd')

    def test_match_get_problem_last(self):
        '''
        Test that match_get_problem matches last problem
        '''

        p = match_get_problem('last')
        self.assertEqual(p.uid, 1234)

    def test_match_get_problem_multiple(self):
        '''
        Test that match_get_problem fails when multiple problems match
        '''

        with captured_output() as (cap_stdout, cap_stderr):
            with self.assertRaises(SystemExit):
                match_get_problem('pavucontrol')

        stdout = cap_stdout.getvalue()
        self.assertIn("Ambiguous", stdout)
        self.assertIn("pavucontrol@bc60a5c", stdout)
        self.assertIn("pavucontrol@acbea5c", stdout)

    def test_match_get_problem_multiple_allowed(self):
        '''
        Test that match_get_problem matches multiple problems when allowed
        '''

        p = match_get_problem('pavucontrol', allow_multiple=True)
        self.assertEqual(len(p), 2)

    def test_match_get_problem_empty_database(self):
        '''
        Test that match_get_problem handles no problems in database
        '''

        import problem

        with clitests.monkey_patch(problem, 'list', lambda *args, **kwargs: []):
            with captured_output() as (cap_stdout, cap_stderr):
                with self.assertRaises(SystemExit):
                    match_get_problem('nope')

            stdout = cap_stdout.getvalue()
            self.assertIn("No problem(s) matched", stdout)

            # Similar with last
            with captured_output() as (cap_stdout, cap_stderr):
                with self.assertRaises(SystemExit):
                    match_get_problem('last')

            stdout = cap_stdout.getvalue()
            self.assertIn("No problems", stdout)

    def test_match_get_problem_nonexistent(self):
        '''
        Test that match_get_problem exits on non-existent problem
        '''

        with self.assertRaises(SystemExit):
            match_get_problem('nope')


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    unittest.main()
