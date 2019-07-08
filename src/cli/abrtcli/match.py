import pathlib
import sys
import problem

from abrtcli.i18n import _
from abrtcli.utils import get_human_identifier, sort_problems


def get_match_data(authenticate=False):
    '''
    Return tuple of two dictionaries: one with components as keys
    and one with short_ids as keys

    Utility function used by match_ functions
    '''

    by_human_id = {}
    by_short_id = {}
    by_path = {}

    for prob in problem.list(auth=authenticate):
        _, val = get_human_identifier(prob)

        if val in by_human_id:
            by_human_id[val].append(prob)
        else:
            by_human_id[val] = [prob]

        if prob.short_id in by_short_id:
            by_short_id[prob.short_id].append(prob)
        else:
            by_short_id[prob.short_id] = [prob]

        by_path[prob.path] = prob

    return by_human_id, by_short_id, by_path


def match_completer(_prefix, _parsed_args, **_kwargs):
    '''
    Completer generator used by cli commands using problem lookup
    '''

    by_human_id, by_short_id, by_path = get_match_data()

    for short_id in by_short_id:
        yield short_id

    for human_id, probs in by_human_id.items():
        if len(probs) == 1:
            yield '{0}'.format(human_id)
        else:
            for prob in probs:
                yield '{0}@{1}'.format(human_id, prob.short_id)

    for path in by_path:
        yield path


def match_lookup(in_arg, authenticate=False):
    '''
    Return problems that match `in_arg` passed on command line
    '''

    if not in_arg:
        problems = sort_problems(problem.list(auth=authenticate))

        return [problems[0]] if problems else []
    elif in_arg == '*':
        return problem.list(auth=authenticate)

    by_human_id, by_short_id, by_path = get_match_data(authenticate=authenticate)

    if '@' in in_arg:
        human_id, short_id = in_arg.split('@', 1)
        matches = [problem
                   for component, problems in by_human_id.items()
                   for problem in problems
                   if human_id == component]

        return list(filter(lambda p: p.short_id == short_id, matches))
    else:
        matches = [problem
                   for component, problems in by_human_id.items()
                   for problem in problems
                   if in_arg == component]
        if matches:
            return matches

        matches = [problems[0]
                   for short_id, problems in by_short_id.items()
                   if in_arg == short_id]
        if matches:
            return matches

    matches = [problem
               for path, problem in by_path.items()
               if pathlib.Path(in_arg).resolve().match(path)]
    if matches:
        return matches

    return []


def match_get_problems(problem_match, authenticate=False):
    '''
    Return problem matching `problem_match` pattern
    or exit if there are no such problems.
    '''

    probs = match_lookup(problem_match, authenticate=authenticate)
    if not probs:
        print(_('No matching problems found'))
        sys.exit(1)
    return probs
