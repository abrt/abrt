from nose import tools

from base import ProblematicTestCase

import problem

class ListTestCase(ProblematicTestCase):
    def test_list(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        tools.ok_(ident in map(lambda x: x._probdir,
            problem.list(False, self.proxy)))

        prob.delete()

    def test_list_all(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        tools.ok_(ident in map(lambda x: x._probdir,
            problem.list(True, self.proxy)))

        prob.delete()

