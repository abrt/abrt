from argparse import Action, ArgumentTypeError
from glob import glob
from importlib import import_module
from os.path import basename, dirname
from reportclient import set_verbosity

from abrtcli import config
from abrtcli.i18n import _

class Command:
    aliases = []
    description = None
    name = None

    def __init__(self, subparsers):
        self._parser = subparsers.add_parser(self.name, aliases=self.aliases,
                                             help=self.description,
                                             description=self.description)

        self._parser.set_defaults(func=self.run)

        # Clone of CountAction in argparse with additional bells and whistles.
        class VerbosityAction(Action):
            def __init__(self, option_strings, dest):
                super().__init__(option_strings=option_strings,
                                 dest=dest,
                                 nargs=0,
                                 default=0,
                                 required=False,
                                 help=_('increase output verbosity'))

                self._verbosity = 0
            def __call__(self, parser, namespace, values, option_string=None):
                self._verbosity += 1

                set_verbosity(self._verbosity)

        self._parser.add_argument('-v', '--verbose', action=VerbosityAction)

    @property
    def parser(self):
        return self._parser

    def add_filter_arguments(self):
        group = self._parser.add_argument_group()

        def uint(string):
            index = int(string)
            if index < 1:
                raise ArgumentTypeError(_('positive non-zero integer expected'))
            return index

        group.add_argument('-N', type=uint, dest='n_latest', metavar='COUNT',
                           help=_('filter last N problems'))
        group.add_argument('-c', '--component', action='append', type=str,
                           dest='components', metavar='COMPONENT',
                           help=_('filter problems with matching component'))
        group.add_argument('-n', '--not-reported', action='store_true',
                           help=_('filter unreported problems'))
        group.add_argument('-s', '--since', type=int, metavar='TIMESTAMP',
                           help=_('filter problems older than the specified timestamp'))
        group.add_argument('-u', '--until', type=int, metavar='TIMESTAMP',
                           help=_('filter problems newer than the specified timestamp'))
        group.add_argument('-x', '--executable', type=str, dest='executables',
                           metavar='EXECUTABLE',
                           help=_('filter problems with matching executable'))

    def add_format_arguments(self, default_pretty='full'):
        group = self._parser.add_argument_group()

        group.add_argument('--format', type=str, help=_('output format'))
        group.add_argument('--pretty', type=str, choices=config.FORMATS,
                           default=default_pretty,
                           help=_('built-in output format'))

    def add_match_argument(self):
        self._parser.add_argument('patterns', nargs='*', type=str,
                                  metavar='PATTERN',
                                  help=_('path to the problem directory, problem ID or wildcard'))

    def run(self, arguments):
        pass


_commands = {}


def wildcard_assumed(arguments):
    if not hasattr(arguments, 'patterns'):
        return False

    return (getattr(arguments, 'n_latest', 0)
            or getattr(arguments, 'components', None)
            or getattr(arguments, 'not_reported', False)
            or getattr(arguments, 'since', 0)
            or getattr(arguments, 'until', 0))


def run_command(command, arguments):
    _commands[command].run(arguments)


def load_commands(subparsers):
    for command in Command.__subclasses__():
        instance = command(subparsers)
        _commands[instance.name] = instance


module_names = [basename(path[:-3])
                for path in glob('%s/*.py' % (dirname(__file__)))
                if path != __file__]

for name in module_names:
    import_module(".%s" % (name), package=__name__)
