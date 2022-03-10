import os
import subprocess
import sys

from problem import Ccpp

from abrtcli import config
from abrtcli.i18n import _
from abrtcli.match import match_get_problems
from abrtcli.utils import remember_cwd

from . import Command, run_command

class Gdb(Command):
    name = 'gdb'
    description = 'run GDB for a problem'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.add_filter_arguments()
        self.add_match_argument()

    def run(self, arguments):
        problems = match_get_problems(arguments.patterns,
                                      authenticate=arguments.authenticate,
                                      executables=arguments.executables,
                                      components=arguments.components,
                                      since=arguments.since,
                                      until=arguments.until,
                                      n_latest=arguments.n_latest,
                                      not_reported=arguments.not_reported)
        for problem in problems:
            if not isinstance(problem, Ccpp):
                print(_('The problem is not of a C/C++ type. Can\'t run GDB'))
                sys.exit(1)

            problem.chown()

            cmd = config.GDB_CMD

            with remember_cwd():
                try:
                    os.chdir(problem.path)
                except OSError:
                    print(_('Permission denied: \'{}\'\n'
                            'If this is a system problem'
                            ' try running this command as root')
                          .format(problem.path))
                    sys.exit(1)
                subprocess.call(cmd, shell=True)
