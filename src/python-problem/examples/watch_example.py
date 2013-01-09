import problem
import logging

logging.basicConfig(level=logging.DEBUG)

def monitor(prob):
    print(prob)
    prob.delete()

pwatch = problem.get_problem_watcher()
pwatch.add_callback(monitor)

try:
    pwatch.run()
except KeyboardInterrupt:
    pwatch.quit()
