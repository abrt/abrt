import sys
import problem

from abrtcli.filtering import (filter_components,
                               filter_executables,
                               filter_ids,
                               filter_not_reported,
                               filter_paths,
                               filter_since_timestamp,
                               filter_until_timestamp)
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


def match_lookup(patterns, authenticate=False, executables=None, components=None,
                 since=None, until=None, n_latest=None, not_reported=False):
    '''
    Return problems that match `in_arg` passed on command line
    '''

    problems = problem.list(auth=authenticate)

    if not patterns:
        return sort_problems(problems)[:1]

    if '*' not in patterns:
        id_matches = filter_ids(problems, patterns)
        path_matches = filter_paths(problems, patterns)

        problems = list(set(id_matches) | set(path_matches))

    problems = filter_executables(problems, executables)
    problems = filter_components(problems, components)

    if since:
        problems = filter_since_timestamp(problems, since)
    if until:
        problems = filter_until_timestamp(problems, until)

    if n_latest:
        problems = sort_problems(problems)[:n_latest]

    if not_reported:
        problems = filter_not_reported(problems)

    return problems


def match_get_problems(patterns, authenticate=False, executables=None,
                       components=None, since=None, until=None, n_latest=None,
                       not_reported=False):
    '''
    Return problem matching `problem_matches` pattern
    or exit if there are no such problems.
    '''

    probs = match_lookup(patterns,
                         authenticate=authenticate,
                         executables=executables,
                         components=components,
                         since=since,
                         until=until,
                         n_latest=n_latest,
                         not_reported=not_reported)
    if not probs:
        print(_('No matching problems found'))
        sys.exit(1)
    return probs
