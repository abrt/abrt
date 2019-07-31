import os
import sys
import subprocess
import functools

import argcomplete
from argh import ArghParser, named, arg, aliases, expects_obj
import problem
import report as libreport
import reportclient
from reportclient import ask_yes_no, set_verbosity

from abrtcli import config, l18n
from abrtcli.l18n import _
from abrtcli.match import match_completer, match_get_problem

from abrtcli.filtering import (filter_not_reported,
                               filter_since_timestamp,
                               filter_until_timestamp)

from abrtcli.utils import (fmt_problems,
                           remember_cwd,
                           run_event,
                           sort_problems)

def arg_verbose(func):
    """
    This is a decorator that adds --verbose command line argument to a command.

    If the command supports the argument, the command must correctly initialize
    reportclient, because we want to propagate the verbosity to called
    functions.
    """

    @functools.wraps(func)
    def abrt_wrapper(args):
        if 'verbose' in args:
            reportclient.verbose += args.verbose
            set_verbosity(reportclient.verbose)

        return func(args)

    argh_wrapper = arg('-v', '--verbose', action='count', default=0,
                       help=_('Print verbose information'))
    return argh_wrapper(abrt_wrapper)


@aliases('bt')
@expects_obj
@arg('MATCH', nargs='?', default='last', completer=match_completer)
@arg_verbose
def backtrace(args):
    prob = match_get_problem(args.MATCH, authenticate=args.authenticate)
    if hasattr(prob, 'backtrace'):
        print(fmt_problems(prob, fmt=config.BACKTRACE_FMT))
    else:
        print(_('Problem has no backtrace'))
        if isinstance(prob, problem.Ccpp):
            ret = ask_yes_no(_('Start retracing process?'))
            if ret:
                retrace(args)
                print(fmt_problems(prob, fmt=config.BACKTRACE_FMT))

backtrace.__doc__ = _('Show backtrace of a problem')


@named('debuginfo-install')
@aliases('di')
@expects_obj
@arg('MATCH', nargs='?', default='last', completer=match_completer)
@arg_verbose
def di_install(args):
    prob = match_get_problem(args.MATCH, authenticate=args.authenticate)
    if not isinstance(prob, problem.Ccpp):
        which = _('This')
        if args.MATCH == 'last':
            which = _('Last')

        print(_('{} problem is not of a C/C++ type. Can\'t install debuginfo')
              .format(which))
        sys.exit(1)

    prob.chown()

    with remember_cwd():
        try:
            os.chdir(prob.path)
        except OSError:
            print(_('Permission denied: \'{}\'\n'
                    'If this is a system problem'
                    ' try running this command as root')
                  .format(prob.path))
            sys.exit(1)
        subprocess.call(config.DEBUGINFO_INSTALL_CMD, shell=True)

di_install.__doc__ = _('Install required debuginfo for given problem')


@expects_obj
@arg('-d', '--debuginfo-install', help='Install debuginfo prior launching gdb')
@arg('MATCH', nargs='?', default='last', completer=match_completer)
@arg_verbose
def gdb(args):
    prob = match_get_problem(args.MATCH, authenticate=args.authenticate)
    if not isinstance(prob, problem.Ccpp):
        which = 'This'
        if args.MATCH == 'last':
            which = 'Last'

        print('{} problem is not of a C/C++ type. Can\'t run gdb'
              .format(which))
        sys.exit(1)

    prob.chown()

    if args.debuginfo_install:
        di_install(args)

    cmd = config.GDB_CMD.format(di_path=config.DEBUGINFO_PATH)

    with remember_cwd():
        try:
            os.chdir(prob.path)
        except OSError:
            print(_('Permission denied: \'{}\'\n'
                    'If this is a system problem'
                    ' try running this command as root')
                  .format(prob.path))
            sys.exit(1)
        subprocess.call(cmd, shell=True)

gdb.__doc__ = _('Run GDB against a problem')


@named('list')
@aliases('ls')
@expects_obj
@arg('--since', type=int,
     help=_('List only the problems more recent than specified timestamp'))
@arg('--until', type=int,
     help=_('List only the problems older than specified timestamp'))
@arg('--fmt', type=str,
     help=_('Output format'))
@arg('--pretty', choices=config.FORMATS, default='medium',
     help=_('Built-in output format'))
@arg('-n', '--not-reported', default=False,
     help=_('List only not-reported problems'))
@arg_verbose
def list_problems(args):
    probs = sort_problems(problem.list(auth=args.authenticate))

    if args.since:
        probs = filter_since_timestamp(probs, args.since)

    if args.until:
        probs = filter_until_timestamp(probs, args.until)

    if args.not_reported:
        probs = filter_not_reported(probs)

    if not args.fmt:
        fmt = config.MEDIUM_FMT
    else:
        fmt = args.fmt

    if args.pretty != 'medium':
        fmt = getattr(config, '{}_FMT'.format(args.pretty.upper()))

    out = fmt_problems(probs, fmt=fmt)

    if out:
        print(out)
    else:
        print(_('No problems'))

    settings = problem.load_conf_file('abrt.conf')
    if settings.get('AutoreportingEnabled', 'no') in ['disabled', 'no', '0', 'off']:
        print()
        print(_('The auto-reporting feature is disabled. '
                'Please consider enabling it by issuing “abrt-auto-reporting enabled” as a user with root privileges.'))


list_problems.__doc__ = _('List problems')


@aliases('i')
@expects_obj
@arg('--fmt', type=str,
     help=_('Output format'))
@arg('--pretty', choices=config.FORMATS, default='full',
     help=_('Built-in output format'))
@arg('MATCH', nargs='?', default='last', completer=match_completer)
@arg_verbose
def info(args):
    prob = match_get_problem(args.MATCH, allow_multiple=True, authenticate=args.authenticate)
    if not args.fmt:
        fmt = config.FULL_FMT
    else:
        fmt = args.fmt

    if args.pretty != 'full':
        fmt = getattr(config, '{}_FMT'.format(args.pretty.upper()))

    print(fmt_problems(prob, fmt=fmt))

info.__doc__ = _('Print information about problem')


@aliases('rm')
@expects_obj
@arg('MATCH', nargs='?', default='last', completer=match_completer)
@arg('-i', help=_('Prompt before removal'), default=False)
@arg('-f', help=_('Do not prompt before removal'), default=False)
@arg_verbose
def remove(args):
    prob = match_get_problem(args.MATCH, authenticate=args.authenticate)
    print(fmt_problems(prob, fmt=config.FULL_FMT))

    ret = True
    if not args.f and (args.i or args.MATCH == 'last'):
        # force prompt for last problem to avoid accidents
        ret = ask_yes_no(_('Are you sure you want to delete this problem?'))

    if ret:
        prob.delete()
        print(_('Removed'))

remove.__doc__ = _('Remove problem')


@aliases('e')
@expects_obj
@arg('MATCH', nargs='?', default='last', completer=match_completer)
@arg('-d', '--delete',
     help=_('Remove problem after reporting'),
     default=False)
@arg('-u', "--unsafe",
     help=_('Ignore security checks to be able to report all problems'),
     default=False)
@arg_verbose
def report(args):
    prob = match_get_problem(args.MATCH, authenticate=args.authenticate)

    if prob.not_reportable and not args.unsafe:
        if reportclient.verbose > 0:
            print(prob.not_reportable_reason)

        print(_('Problem \'{0}\' cannot be reported').format(prob.short_id))
        sys.exit(1)

    flags = libreport.LIBREPORT_WAIT | libreport.LIBREPORT_RUN_CLI
    if args.unsafe:
        flags |= libreport.LIBREPORT_IGNORE_NOT_REPORTABLE

    prob.chown()

    libreport.report_problem_in_dir(prob.path, flags)

    if args.delete:
        prob.delete(prob.path)

report.__doc__ = _('Report problem')


@expects_obj
@arg('-l', '--local', action='store_true',
     help=_('Perform local retracing'))
@arg('-r', '--remote', action='store_true',
     help=_('Perform remote retracing using retrace server'))
@arg('-f', '--force', action='store_true',
     help=_('Force retracing even if backtrace already exists'))
@arg('MATCH', nargs='?', default='last', completer=match_completer)
@arg_verbose
def retrace(args):
    # we might not get these var if called from backtrace
    local = getattr(args, 'local', False)
    remote = getattr(args, 'remote', False)
    force = getattr(args, 'force', False)

    prob = match_get_problem(args.MATCH, authenticate=args.authenticate)
    if hasattr(prob, 'backtrace') and not force:
        print(_('Problem already has a backtrace'))
        print(_('Run abrt retrace with -f/--force to retrace again'))
        ret = ask_yes_no(_('Show backtrace?'))
        if ret:
            print(fmt_problems(prob, fmt=config.BACKTRACE_FMT))
    elif not isinstance(prob, problem.Ccpp):
        print(_('No retracing possible for this problem type'))
    else:
        if not (local or remote):  # ask..
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

        prob.chown()

        if remote:
            print(_('Remote retracing'))
            run_event('analyze_RetraceServer', prob)
        else:
            print(_('Local retracing'))
            run_event('analyze_LocalGDB', prob)


retrace.__doc__ = _('Generate backtrace from coredump')


@aliases('st')
@expects_obj
@arg('-b', '--bare', action='store_true',
     help=_('Print only the problem count without any message'))
@arg('-s', '--since', type=int,
     help=_('Print only the problems more recent than specified timestamp'))
@arg('-n', '--not-reported', default=False,
     help=_('List only not-reported problems'))
@arg('-q', '--quiet', default=False,
     help=_('Only display output when a problem had been detected'))
@arg_verbose
def status(args):
    probs = problem.list(auth=args.authenticate)

    since_append = ''
    if args.since:
        probs = filter_since_timestamp(probs, args.since)
        since_append = ' --since {}'.format(args.since)

    if args.not_reported:
        probs = filter_not_reported(probs)

    if args.bare:
        print(len(probs))
        return

    if not args.quiet or len(probs) > 0:
        print(_('ABRT has detected {} problem(s). For more info run: abrt list{}')
              .format(len(probs), since_append))

status.__doc__ = _('Print count of the recent crashes')


def main():
    l18n.init()

    parser = ArghParser()
    parser.add_argument('-a', '--authenticate',
                        action='store_true',
                        help=_('Authenticate and show all problems'
                               ' on this machine'))

    parser.add_argument('-v', '--version',
                        action='version',
                        version=config.VERSION)

    parser.add_commands([
        backtrace,
        di_install,
        gdb,
        info,
        list_problems,
        remove,
        report,
        retrace,
        status,
    ])

    argcomplete.autocomplete(parser)

    try:
        parser.dispatch()
    except KeyboardInterrupt:
        sys.exit(1)

    sys.exit(0)
