import sys

from problem.exception import InvalidProblem
import report
import reportclient

from abrtcli.i18n import _
from abrtcli.match import match_get_problems

from . import Command

class Report(Command):
    aliases = ['e']
    name = 'report'
    description = 'report problem'

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        self.add_filter_arguments()
        self.add_match_argument()

        self.parser.add_argument('-d', '--delete', action='store_true',
                                 help=_('remove after reporting'))
        self.parser.add_argument('--unsafe', action='store_true',
                                 help=_('ignore unreportable state'))

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
            try:
                if problem.not_reportable and not arguments.unsafe:
                    if reportclient.verbose > 0:
                        print(problem.not_reportable_reason)

                    print(_('Problem \'{0}\' cannot be reported').format(problem.short_id))
                    sys.exit(1)

                flags = report.LIBREPORT_WAIT | report.LIBREPORT_RUN_CLI
                if arguments.unsafe:
                    flags |= report.LIBREPORT_IGNORE_NOT_REPORTABLE

                problem.chown()

                print(_("Reporting problem %s\n" % (problem.short_id)))

                report.report_problem_in_dir(problem.path, flags)

                if arguments.delete:
                    problem.delete(problem.path)
            except InvalidProblem:
                print(_("Problem '{0}' cannot be reported. The problem directory '{1}' is gone")
                      .format(problem.short_id, problem.path))
                sys.exit(1)
