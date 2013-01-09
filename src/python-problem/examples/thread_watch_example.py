from __future__ import print_function

import sys
import time
import problem
import threading

class ProblemWatchThread(threading.Thread):
    def __init__(self):
        super(ProblemWatchThread, self).__init__()
        self.pwatch = problem.get_problem_watcher()
        self.pwatch.add_callback(self.handle)
        self.probcount = 0

    def handle(self, prob):
        self.probcount += 1
        print('{0}: {1}'.format(self.probcount, prob))
        # prob.delete()

    def run(self):
        self.pwatch.run()

    def stop(self):
        self.pwatch.quit()

pwt = ProblemWatchThread()
pwt.start()

i = 0
print('Waiting for new problem to appear')
spinner = ['\\', '|', '/', '-']

try:
    while True:
        time.sleep(0.1)
        print('{0}\r'.format(spinner[i]), end='')
        i += 1
        i = i % len(spinner)
        sys.stdout.flush()
except KeyboardInterrupt:
    pwt.stop()

pwt.stop()
