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

        group.add_argument('-l', '--local', action='store_true',
                           help=_('perform retracing locally'))
        group.add_argument('-r', '--remote', action='store_true',
                           help=_('submit core dump for remote retracing'))

        self.parser.add_argument('-f', '--force', action='store_true',
                                 help=_('force retracing'))

    def run(self, arguments):
        # We don’t get these bad boys when invoked by the “backtrace” command.
        local = getattr(arguments, 'local', False)
        remote = getattr(arguments, 'remote', False)
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
                if not (local or remote):
                    ret = ask_yes_no(
                        _('Upload core dump and perform remote'
                          ' retracing? (It may contain sensitive data).'
                          ' If your answer is \'No\', a stack trace will'
                          ' be generated locally. Local retracing'
                          ' requires downloading potentially large amount'
                          ' of debuginfo data'))

                    if ret:
                        remote = True
                    else:
                        local = True

                problem.chown()

                if remote:
                    print(_('Remote retracing'))
                    run_event('analyze_RetraceServer', problem)
                else:
                    print(_('Local retracing'))
                    run_event('analyze_LocalGDB', problem)
