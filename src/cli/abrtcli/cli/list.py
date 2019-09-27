from abrtcli import config
from abrtcli.i18n import _
from abrtcli.match import match_lookup
from abrtcli.utils import format_problems

from . import Command

class List(Command):
    aliases = ['ls']
    name = 'list'
    description = 'list problems'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.add_filter_arguments()
        self.add_format_arguments(default_pretty='medium')

    def run(self, arguments):
        problems = match_lookup(['*'],
                                authenticate=arguments.authenticate,
                                executables=arguments.executables,
                                components=arguments.components,
                                since=arguments.since,
                                until=arguments.until,
                                n_latest=arguments.n_latest,
                                not_reported=arguments.not_reported)
        if problems:
            fmt = arguments.format
            if not fmt:
                fmt = getattr(config, '%s_FMT' % (arguments.pretty.upper()))
            print(format_problems(problems, fmt=fmt))
        else:
            print(_('No problems'))
