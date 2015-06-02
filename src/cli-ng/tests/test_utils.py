#!/usr/bin/python
# -*- encoding: utf-8 -*-
import logging
try:
    import unittest2 as unittest
except ImportError:
    import unittest

import os

import clitests

import problem

from abrtcli.config import ONELINE_FMT

from abrtcli.utils import (captured_output,
                           fmt_problems,
                           get_problem_field,
                           get_human_identifier,
                           remember_cwd,
                           sort_problems,
                           upcase_first_letter)


class UtilsTestCase(clitests.TestCase):
    '''
    Tests for utility functions
    '''

    def test_fmt_problems(self):
        '''
        Test default problem formatting
        '''

        pl = problem.list()
        res = fmt_problems(pl)

        for prob in pl:
            self.assertIn(prob.short_id, res)
            field, value = get_human_identifier(prob)
            self.assertIn(value, res)
            self.assertIn(str(prob.count), res)

        self.assertIn('Bugzilla', res)
        self.assertIn('https://bugzilla.redhat.com/show_bug.cgi?id=1223349',
                      res)

        self.assertIn('ABRT Server', res)
        furl = 'https://retrace.fedoraproject.org/faf/reports/bthash/' \
               '3505a6db8a6bd51a3d690f1553b'
        self.assertIn(furl, res)

        self.assertIn('Not reportable', res)
        self.assertIn('Not reportable reason', res)

    def test_fmt_problems_oneline(self):
        '''
        Test oneline problem formatting
        '''

        pl = problem.list()
        res = fmt_problems(pl, fmt=ONELINE_FMT)

        self.assertIn('bc60a5c 15x pavucontrol', res)
        self.assertIn('ffe635c 1x /home/user/bin/user_app', res)

    def test_fmt_problems_custom(self):
        '''
        Test custom problem formatting
        '''

        pl = problem.list()
        fmt = '''#table|id,{short_id}|user id,{uid_username}| '''
        res = fmt_problems(pl, fmt=fmt)

        self.assertIn('User id', res)
        self.assertIn('1234', res)
        self.assertTrue(len(res.splitlines()) > len(pl))

    def test_fmt_problems_custom_oneline(self):
        '''
        Test custom problem formatting
        '''

        pl = problem.list()
        fmt = '''{short_id} {uid_username}'''
        res = fmt_problems(pl, fmt=fmt)

        self.assertIn('1234', res)
        self.assertTrue(len(res.splitlines()) == len(pl))

    def test_fmt_problems_empty_input(self):
        '''
        Test that fmt_problems handles None as problem list
        '''

        self.assertEqual(fmt_problems(None), '')

    def test_get_problem_field(self):
        p = problem.list()[0]
        self.assertTrue(get_problem_field(p, 'count'), p.count)
        self.assertEqual(get_problem_field(p, 'notavail'), None)

    def test_get_human_identifier(self):
        p0 = problem.list()[0]
        p3 = problem.list()[3]
        p4 = problem.list()[4]
        p0_t, p0_v = get_human_identifier(p0)
        p3_t, p3_v = get_human_identifier(p3)
        p4_t, p4_v = get_human_identifier(p4)
        self.assertEqual(p0_t, 'component')
        self.assertEqual(p0_v,  p0.component)
        self.assertEqual(p3_t, 'executable')
        self.assertEqual(p3_v,  p3.executable)
        self.assertEqual(p4_t, 'type')
        self.assertEqual(p4_v,  p4.type)

    def test_sort_problems(self):
        '''
        Test if problems are sotred by time
        '''

        pl = problem.list()
        spl = sort_problems(pl)
        self.assertTrue(spl[-1] == pl[2])
        self.assertTrue(spl[0] == pl[3])

    def test_upcase_first_letter(self):
        self.assertEqual('LaLa', upcase_first_letter('laLa'))

    def test_remember_cwd(self):
        cwd = os.getcwd()
        with remember_cwd():
            os.chdir('/tmp')
        self.assertEqual(os.getcwd(), cwd)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    unittest.main()
