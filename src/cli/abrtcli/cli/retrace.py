from problem import Ccpp
from reportclient import ask_yes_no

from abrtcli import config
from abrtcli.i18n import _
from abrtcli.match import match_get_problems
from abrtcli.utils import format_problems, run_event

from . import Command

class Retrace(Command):
    name = 'retrace'
    description = 'generate stack trace from core dump'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.add_filter_arguments()
        self.add_match_argument()

        group = self.parser.add_mutually_exclusive_group()

        self.parser.add_argument('-f', '--force', action='store_true',
                                 help=_('force retracing'))

    def run(self, arguments):
        # We don’t get these bad boys when invoked by the “backtrace” command.
        force = getattr(arguments, 'force', False)

        problems = match_get_problems(arguments.patterns,
                                      authenticate=arguments.authenticate,
                                      executables=arguments.executables,
                                      components=arguments.components,
                                      since=arguments.since,
                                      until=arguments.until,
                                      n_latest=arguments.n_latest,
                                      not_reported=arguments.not_reported)
        for problem in problems:
            if hasattr(problem, 'backtrace') and not force:
                print(_('Problem already has a backtrace'))
                print(_('Run abrt retrace with -f/--force to retrace again'))
                if ask_yes_no(_('Show backtrace?')):
                    print(format_problems(problem, fmt=config.BACKTRACE_FMT))
            elif not isinstance(problem, Ccpp):
                print(_('No retracing possible for this problem type'))
            else:
                problem.chown()

                # Only local retracing is supported.
                run_event('analyze_LocalGDB', problem)
