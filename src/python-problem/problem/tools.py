import problem


def problemify(probdir, proxy):
    by_typ = dict(zip(problem.PROBLEM_TYPES.values(),
                      problem.PROBLEM_TYPES.keys()))

    typ = proxy.get_item(probdir, 'type')
    reason = proxy.get_item(probdir, 'reason')

    if typ not in by_typ:
        return problem.Unknown(reason)

    class_name = by_typ[typ].lower().capitalize()

    prob = getattr(problem, class_name)(reason)
    prob._probdir = probdir
    prob._persisted = True
    prob._proxy = proxy
    return prob
