import problem

for prob in problem.list():
    assert prob.type == problem.CCPP
    del prob.coredump
    prob.cmdline = prob.cmdline.replace('will_segfault', '31337')
    prob.save()
