#!/usr/bin/python3
# -*- encoding: utf-8 -*-
import datetime
import logging
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

    def test_match_get_problems(self):
        with self.assertRaises(SystemExit):
            match_get_problems(['adun toridas'])

    def test_match_combo(self):
        '''
        Test matching based on combinations of criteria
        '''
        since = datetime.datetime(2015, 5, 1)
        until = datetime.datetime(2015, 7, 1)
        matches = match_lookup(['bc60a5cbddb4e3667511e718ceecac16133acc97'],
                               since=since.timestamp(),
                               until=until.timestamp(),
                               not_reported=True)
        self.assertEqual(len(matches), 1)
        matches = match_lookup(['bc60a5cbddb4e3667511e718ceecac16133acc97'],
                               since=since.timestamp(),
                               until=until.timestamp())
        self.assertEqual(len(matches), 2)
        matches = match_lookup(['bc60a5cbddb4e3667511e718ceecac16133acc97'],
                               components=['pavucontrol'],
                               since=since.timestamp(),
                               n_latest=1)
        self.assertEqual(len(matches), 1)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    unittest.main()
