import problem

prob = problem.Selinux(reason='Front fell off')

prob.executable = '/usr/bin/time'

prob.save()
