import datetime


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
