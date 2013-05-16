import problem

prob = problem.Runtime(
        reason='random_error_message: assertion "error" failed',
    )

prob.add_current_process_data()
prob.add_current_environment()
prob.executable = '/usr/bin/will_abort'
prob.save()
