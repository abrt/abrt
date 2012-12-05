import datetime

import problem

class FakeProxy(object):
    data = dict()

    def get_item(self, dump_dir, name):
        if dump_dir not in self.data:
            raise problem.exception.InvalidProblem()

        try:
            return self.data[dump_dir][name]
        except KeyError:
            return None

    def set_item(self, dump_dir, name, value):
        self.data[dump_dir][name] = value

    def del_item(self, dump_dir, name):
        del self.data[dump_dir][name]

    def create(self, problem_dict):
        datestr = str(datetime.datetime.now()).replace(' ', '-')
        name = '{0}-{1}'.format(problem_dict['type'], datestr)
        self.data[name] = problem_dict
        return name

    def delete(self, dump_dir):
        del self.data[dump_dir]

    def list(self):
        return self.data.keys()

    def list_all(self):
        return self.data.keys()
