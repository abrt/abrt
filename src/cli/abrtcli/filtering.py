import datetime
import pathlib


def filter_components(problems, components):
    '''
    Return problems with matching components
    '''
    if not components:
        return problems

    def predicate(problem):
        if not hasattr(problem, 'component'):
            return False
        return problem.component in components

    return list(filter(predicate, problems))


def filter_executables(problems, executables):
    '''
    Return problems with matching executables
    '''
    if not executables:
        return problems

    def predicate(problem):
        if not hasattr(problem, 'executable'):
            return False
        return problem.executable in executables

    return list(filter(predicate, problems))


def filter_ids(problems, ids):
    '''
    Return problems with matching IDs
    '''
    if not ids:
        return problems

    return list(filter(lambda x: x.short_id in ids or x.id in ids, problems))


def filter_paths(problems, paths):
    '''
    Return problems with matching paths
    '''
    if not paths:
        return problems

    def predicate(problem):
        for path in paths:
            # See whether the path is an actual path ...
            if pathlib.Path(problem.path) == pathlib.Path(path).resolve():
                return True
        for path in paths:
            try:
                # ... failing that, see whether it can be interpreted as a glob
                if pathlib.Path(problem.path).match(path):
                    return True
            # happens whem a lone '.' is being used as a glob
            except ValueError:
                return False
        return False

    return list(filter(predicate, problems))


def filter_since(probs, since):
    '''
    Return problems that occurred `since` datetime
    '''

    return list(filter(lambda x: x.time > since, probs))


def filter_since_timestamp(probs, since_ts):
    '''
    Return problems that occurred `since` timestamp
    '''

    since = datetime.datetime.fromtimestamp(float(since_ts))
    return list(filter_since(probs, since))


def filter_until(probs, until):
    '''
    Return problems that occurred `until` datetime
    '''

    return list(filter(lambda x: x.time < until, probs))


def filter_until_timestamp(probs, until_ts):
    '''
    Return problems that occurred `until` timestamp
    '''

    until = datetime.datetime.fromtimestamp(float(until_ts))
    return list(filter_until(probs, until))


def filter_reported(probs):
    '''
    Return only reported problems
    '''

    return list(filter(lambda x: hasattr(x, 'reported_to'), probs))


def filter_not_reported(probs):
    '''
    Return only non-reported problems
    '''

    return list(filter(lambda x: not hasattr(x, 'reported_to'), probs))
