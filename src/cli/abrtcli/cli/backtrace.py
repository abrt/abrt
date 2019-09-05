from problem import Ccpp
from reportclient import ask_yes_no

from abrtcli import config
from abrtcli.match import match_get_problems
from abrtcli.i18n import _
from abrtcli.utils import format_problems

from . import Command, run_command

class Info(Command):
    aliases = ['bt']
    name = 'backtrace'
    description = 'show backtrace of a problem'

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
            if hasattr(problem, 'backtrace'):
                print(format_problems(problem, fmt=config.BACKTRACE_FMT))
            else:
                print(_('Problem has no backtrace'))
                if isinstance(problem, Ccpp):
                    if ask_yes_no(_('Start retracing process?')):
                        run_command('retrace', arguments)
                        print(format_problems(problem, fmt=config.BACKTRACE_FMT))
