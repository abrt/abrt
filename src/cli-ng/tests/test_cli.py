#!/usr/bin/python3
# -*- encoding: utf-8 -*-
import logging
try:
    import unittest2 as unittest
except ImportError:
    import unittest

import clitests

from abrtcli.utils import captured_output


class CliTestCase(clitests.TestCase):
    '''
    Tests for cli functions
    '''

    def test_cli_sanity(self):
        '''
        Test if main works and there are no import/decorator errors
        '''

        with captured_output() as (cap_stdout, cap_stderr):

            # we have to import here
            # otherwise argh.dispatch stores sys.std* on import
            # and ignores our override

            from abrtcli.cli import main
            with self.assertRaises(SystemExit):
                main()

        out = cap_stderr.getvalue()
        out += cap_stdout.getvalue()
        self.assertIn("usage", out)
        self.assertIn("debuginfo-install", out)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    unittest.main()
