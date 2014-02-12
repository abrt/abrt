import problem

for prob in problem.list():
    assert prob.type == problem.CCPP
    prob.delete()
