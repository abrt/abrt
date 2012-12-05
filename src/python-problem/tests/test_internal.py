from nose import tools

from base import ProblematicTestCase

import problem

class InternalProblemImplementationTestCase(ProblematicTestCase):
    def test_init(self):
        prob = self.create_problem()
        tools.eq_(prob._proxy, self.proxy )

    def test_setattr(self):
        prob = self.create_problem()

        prob.test = 0
        tools.eq_(prob._data['test'], 0)
        tools.eq_(prob._dirty_data, {})

        prob._test = 1
        tools.eq_(prob._test, 1)
        tools.ok_('_test' not in prob._data)

        prob.add_current_process_data()
        prob.save()

        prob.persisted_test = 0
        tools.eq_(prob._data['persisted_test'], 0)
        tools.eq_(prob._dirty_data['persisted_test'], 0)

        prob.delete()

    def test_setitem(self):
        prob = self.create_problem()

        prob['test'] = 0
        tools.eq_(prob._data['test'], 0)
        tools.eq_(prob._dirty_data, {})

        prob['_test'] = 1
        tools.ok_('_test' not in prob._data)

        prob.add_current_process_data()
        prob.save()

        prob['persisted_test'] = 0
        tools.eq_(prob._data['persisted_test'], 0)
        tools.eq_(prob._dirty_data['persisted_test'], 0)

        prob.delete()

    def test_delattr(self):
        prob = self.create_problem()
        del prob.reason
        tools.ok_('reason' not in prob._data)

        prob.add_current_process_data()
        prob.save()

        del prob.type
        tools.eq_(prob._dirty_data, {'type': None})

        prob.save()

        tools.eq_(prob._dirty_data, {})

        prob.delete()

    def test_delitem(self):
        prob = self.create_problem()
        del prob['reason']
        tools.ok_('reason' not in prob._data)

        prob.add_current_process_data()
        prob.save()

        del prob['type']
        tools.eq_(prob._dirty_data, {'type': None})

        prob.save()

        tools.eq_(prob._dirty_data, {})

        prob.delete()

    def test_items(self):
        prob = problem.Runtime('Massive error')
        tools.eq_(prob.items(), prob._data.items())

    def test_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        tools.eq_(prob._probdir, ident)

        prob.delete()

    def test_dirty_save(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        prob.executable = 'nine'

        tools.eq_(prob._dirty_data['executable'], 'nine')
        prob.save()
        tools.eq_(prob._dirty_data, {})

        prob.delete()

    def test_delete(self):
        prob = self.create_problem()
        prob.add_current_process_data()
        ident = prob.save()

        prob.delete()

        tools.eq_(prob._persisted, False)
        tools.eq_(prob._probdir, None)
        tools.eq_(prob._dirty_data, {})
