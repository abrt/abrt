import problem

for prob in problem.list():
    assert prob.type == problem.CCPP
    del prob.coredump
    prob.core_backtrace = prob.core_backtrace.replace('will_segfault', '31337')
    prob.save()
