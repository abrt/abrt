import problem

def problemify(probdir, proxy):
    by_analyzer = dict(zip(problem.PROBLEM_TYPES.values(), 
        problem.PROBLEM_TYPES.keys()))

    analyzer = proxy.get_item(probdir, 'analyzer')
    reason = proxy.get_item(probdir, 'reason')

    if analyzer not in by_analyzer:
        return problem.Unknown(reason)

    class_name = by_analyzer[analyzer].lower().capitalize()

    prob = getattr(problem, class_name)(reason)
    prob._probdir = probdir
    prob._persisted = True
    prob._proxy = proxy
    return prob
