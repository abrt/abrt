from abrtcli import config
from abrtcli.match import match_get_problems
from abrtcli.utils import format_problems

from . import Command

class Info(Command):
    aliases = ['i']
    name = 'info'
    description = 'show problem information'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.add_filter_arguments()
        self.add_format_arguments()
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

        fmt = getattr(config, '{}_FMT'.format(arguments.pretty.upper()))

        print(format_problems(problems, fmt=fmt))
