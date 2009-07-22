class IsRunning(Exception):
    def __init__(self):
        self.what = "Another client is already running, trying to wake it."
    def __str__(self):
        return self.what

