import problem

for prob in problem.list():
    print(prob)
    print(repr(prob.time))
    if hasattr(prob, 'pid'):
        print(prob.pid)
