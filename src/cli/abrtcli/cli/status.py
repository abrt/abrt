from abrtcli.match import match_lookup
from abrtcli.i18n import _, N_

from . import Command

class Status(Command):
    aliases = ['st']
    name = 'status'
    description = 'show the number of detected problems'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.add_filter_arguments()

        self.parser.add_argument('-b', '--bare', action='store_true',
                                 help=_('show only the number of problems'))
        self.parser.add_argument('-q', '--quiet', action='store_true',
                                 help=_('only show output if at least one problem is found'))

    def run(self, arguments):
        problems = match_lookup(['*'],
                                authenticate=arguments.authenticate,
                                executables=arguments.executables,
                                components=arguments.components,
                                since=arguments.since,
                                until=arguments.until,
                                n_latest=arguments.n_latest,
                                not_reported=arguments.not_reported)

        since_append = ''
        if arguments.since:
            since_append = ' --since {}'.format(arguments.since)

        if arguments.bare:
            print(len(problems))
            return

        if not arguments.quiet or problems:
            print(N_('ABRT has detected a problem. For more information, run “abrt list{}”',
                     'ABRT has detected %d problems. For more information, run “abrt list{}”' % (len(problems)),
                     len(problems))
                  .format(since_append))
