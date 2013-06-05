import problem

for prob in problem.list(auth=True):
    print(prob)
    if hasattr(prob, 'username'):
        print('Problem belongs to {0}'.format(prob.username))
