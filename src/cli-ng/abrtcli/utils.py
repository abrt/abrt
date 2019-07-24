import os
import re
import sys
import contextlib
from functools import reduce
try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO

from abrtcli.l18n import _
from abrtcli.config import MEDIUM_FMT
import report

braces_re = re.compile(r'\{([^}]+)\}')


def as_table(data, margin=2, separator=' '):
    '''
    Return `data` formatted as table.
    '''

    data = list(map(lambda x: list(map(str, x)), data))

    widths = reduce(
        lambda x, y: list(map(
            lambda t: max(t[0], t[1]), zip(x, y)
        )),
        map(lambda x: map(len, x), data),
        map(lambda _: 0, data))

    fmt = ''
    for num, width in enumerate(widths):
        if num != 0:
            width = 0
        fmt += '{{{0}:<{1}}}{2}'.format(num, width, separator * margin)
    fmt += '\n'

    return ''.join(map(lambda row: fmt.format(*row), data))[:-1]


def fmt_problems(probs, fmt=MEDIUM_FMT):
    '''
    Return preformatted problem data of `prob` according to `fmt`
    '''

    if probs is None:
        return ''

    if not isinstance(probs, list):
        probs = [probs]

    fmt = fmt.replace('|', '\n')
    tabular = '#table' in fmt
    oneline = '\n' not in fmt

    out = ''
    for prob in probs:
        what_field, what = get_human_identifier(prob)

        context_vars = {
            'what': what,
            'what_field': what_field,
        }

        uid = get_problem_field(prob, 'uid')
        if uid is not None:
            username = get_problem_field(prob, 'username')
            if username:
                uid_username = ('{0} ({1})'
                                .format(uid, username))
            else:
                uid_username = str(uid)

            context_vars['uid_username'] = uid_username

        if prob.not_reportable:
            context_vars['not_reportable'] = _('Not reportable')
            reason = prob.not_reportable_reason.rstrip()

            if tabular:
                reason = reason.replace('\n', '\n,')

            context_vars['not_reportable_reason'] = reason

        if hasattr(prob, 'reported_to'):
            r_out = ''
            rtl = prob.reported_to.splitlines()

            # each reported to line item as separate row
            # except for BTHASH
            for rline in rtl:
                if 'BTHASH' in rline:
                    continue

                if 'URL=' in rline:
                    rep_to, url = rline.split('URL=', 1)
                    if ': ' in rep_to:
                        rep_to, rest = rep_to.split(': ')

                    if tabular:
                        r_out += '\n{},{}'.format(rep_to, url)

            if r_out:
                context_vars['reported_to'] = r_out

        if hasattr(prob, 'backtrace'):
            if not oneline:
                context_vars['backtrace'] = '\n' + prob.backtrace

        if not hasattr(prob, 'count'):
            context_vars['count'] = 1

        sfmt = fmt.splitlines()
        for line in sfmt:
            if not line.rstrip():
                if tabular:
                    out += ',\n'
                elif not oneline:
                    out += '\n'
                continue

            if line[0] == '#':  # output mode selector or comment
                continue

            template_vars = braces_re.findall(line)

            missing_var = False
            for var in template_vars:
                # try looking up missing context var in problem items
                if var not in context_vars:
                    val = get_problem_field(prob, var)
                    if val:
                        context_vars[var] = val
                    else:
                        missing_var = True
                        context_vars[var] = ''

            if not missing_var or oneline:
                fmtline = line.format(**context_vars)
                if not oneline:
                    fmtline = upcase_first_letter(fmtline)

                out += fmtline + '\n'

        # separator
        if tabular:
            out += ',\n'
        elif not oneline:
            out += '\n'

    if tabular:
        rows = out.splitlines()
        rows = map(lambda x: x.split(',', 1), rows)
        out = as_table(list(rows)[:-1])
    else:
        out = out.replace('\n\n', '\n')

    return out.rstrip()


def get_problem_field(prob, field):
    '''
    Return problem field `field` or None
    '''

    try:
        return getattr(prob, field)
    except AttributeError:
        return None


def get_human_identifier(prob):
    '''
    Return first found problem field from list of candidate fields
    that will be used as a problem identifier for humans
    '''

    candidates = ['component', 'executable', 'type']

    for c in candidates:
        val = get_problem_field(prob, c)
        if val:
            return c, val

    return None, None


def sort_problems(probs, recent_first=True):
    '''
    Sort problems by time, `recent_first` by default
    '''

    return sorted(probs, key=lambda p: p.time, reverse=recent_first)


def upcase_first_letter(s):
    '''
    UC First
    '''
    return s[0].upper() + s[1:]


def run_event(event_name, prob):
    '''
    Run event with `event_name` on problem `prob`
    '''

    state, ret = report.run_event_on_problem_dir(prob.path, event_name)

    if ret == 0 and state.children_count == 0:
        sys.stderr.write('No actions were found for event {}'
                         .format(event_name))
        return False

    return True


@contextlib.contextmanager
def remember_cwd():
    curdir = os.getcwd()
    try:
        yield
    finally:
        os.chdir(curdir)


@contextlib.contextmanager
def captured_output():
    """
    Capture stdout and stderr output of the executed block

    Example:

    with captured_output() as (out, err):
        foo()
    """

    new_out, new_err = StringIO(), StringIO()
    old_out, old_err = sys.stdout, sys.stderr
    try:
        sys.stdout, sys.stderr = new_out, new_err
        yield sys.stdout, sys.stderr
    finally:
        sys.stdout, sys.stderr = old_out, old_err
