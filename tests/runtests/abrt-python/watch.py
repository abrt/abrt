import sys
import problem


def monitor(prob):
    print(prob)
    sys.stdout.flush()

pwatch = problem.get_problem_watcher()
pwatch.add_callback(monitor)
pwatch.run()
