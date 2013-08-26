import problem

bugs = set()

for prob in problem.list():
    if not hasattr(prob, 'reported_to'):
        continue

    for line in prob.reported_to.splitlines():
        if line.startswith('Bugzilla:'):
            bug_num = int(line.split('=')[-1])
            bugs.add(bug_num)

print(bugs)
