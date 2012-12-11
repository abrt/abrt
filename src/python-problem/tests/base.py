import unittest

from util import FakeProxy

import problem

class ProblematicTestCase(unittest.TestCase):
    def setUp(self):
        self.proxy = FakeProxy()
        #self.proxy = problem.proxies.get_proxy()
        #self.proxy = problem.proxies.FsProxy('/tmp/abrt/')

    def create_problem(self):
        prob = problem.Runtime(reason='Front fell off')
        prob._proxy = self.proxy
        return prob
