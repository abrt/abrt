import problem

prob = problem.Runtime(
        reason='egg_error_message: assertion "error" failed',
    )

prob.add_current_process_data()
prob.add_current_environment()
prob.save()
