import sys
import problem

from abrtcli.l18n import _
from abrtcli.utils import get_human_identifier, sort_problems


def get_match_data(auth=False):
    '''
    Return tuple of two dictionaries: one with components as keys
    and one with short_ids as keys

    Utility function used by match_ functions
    '''

    by_human_id = {}
    by_short_id = {}

    for prob in problem.list(auth=auth):
        comp_or_exe, val = get_human_identifier(prob)

        if val in by_human_id:
            by_human_id[val].append(prob)
        else:
            by_human_id[val] = [prob]

        if prob.short_id in by_short_id:
            by_short_id[prob.short_id].append(prob)
        else:
            by_short_id[prob.short_id] = [prob]

    return by_human_id, by_short_id


def match_completer(prefix, parsed_args, **kwargs):
    '''
    Completer generator used by cli commands using problem lookup
    '''

    by_human_id, by_short_id = get_match_data()

    for short_id in by_short_id.keys():
        yield short_id

    for human_id, probs in by_human_id.items():
        if len(probs) == 1:
            yield '{0}'.format(human_id)
        else:
            for prob in probs:
                yield '{0}@{1}'.format(human_id, prob.short_id)


def match_lookup(in_arg, auth=False):
    '''
    Return problems that match `in_arg` passed on command line
    '''

    by_human_id, by_short_id = get_match_data(auth=auth)

    res = None

    if in_arg in by_human_id:
        res = by_human_id[in_arg]
    elif in_arg in by_short_id:
        res = by_short_id[in_arg]
    elif '@' in in_arg:
        human_id, short_id = in_arg.split('@', 1)

        if human_id in by_human_id:
            probs = by_human_id[human_id]

            res = list(filter(lambda p: p.short_id == short_id, probs))

    return res


def match_get_problem(problem_match, allow_multiple=False, auth=False):
    '''
    Return problem matching `problem_match` pattern
    or exit if there are no such problems or pattern
    results in multiple problems (unless `allow_multiple` is set
    to True).
    '''

    prob = None
    if problem_match == 'last':
        probs = sort_problems(problem.list(auth=auth))
        if not probs:
            print(_('No problems'))
            sys.exit(0)

        prob = probs[0]
    else:
        probs = match_lookup(problem_match, auth=auth)
        if not probs:
            print(_('No problem(s) matched'))
            sys.exit(1)
        elif len(probs) > 1:
            if allow_multiple:
                return probs

            match_collision(probs)
            sys.exit(1)
        else:
            prob = probs[0]

    return prob


def match_collision(probs):
    '''
    Handle matches that result in multiple problems by telling user
    to be more specific
    '''

    print(_('Ambiguous match specified resulting in multiple problems:'))
    for prob in probs:
        field, val = get_human_identifier(prob)
        print('- {}@{} ({})'.format(val, prob.short_id, prob.time))
