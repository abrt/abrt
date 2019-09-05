from reportclient import ask_yes_no

from abrtcli import config
from abrtcli.i18n import _
from abrtcli.match import match_get_problems
from abrtcli.utils import format_problems

from . import Command

class Remove(Command):
    aliases = ['rm']
    name = 'remove'
    description = 'remove problem'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.add_filter_arguments()
        self.add_match_argument()

        self.parser.add_argument('-f', '--force', action='store_true',
                                 help=_('do not prompt before removal'))

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
            print(format_problems(problem, fmt=config.FULL_FMT), '\n')

            if not arguments.force:
                if not ask_yes_no(_('Are you sure you want to delete this problem?')):
                    continue

            problem.delete()
            print(_('Removed'), '\n')
