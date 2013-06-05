import problem

for prob in problem.list():
    if prob.type == problem.JAVA:
        prob.delete()

    if prob.type == problem.CCPP:
        if 'password' in prob.backtrace:
            del prob.backtrace
            prob.save()

    if prob.type == problem.KERNELOOPS:
        prob.backtrace = prob.backtrace.replace(' ?', '')
        prob.save()
