from abrt_utils import _, log, log1, log2

class IsRunning(Exception):
    def __init__(self):
        Exception.__init__(self)
        self.what = _("Another client is already running, trying to wake it...")
    def __str__(self):
        return self.what

class WrongData(Exception):
    def __init__(self):
        Exception.__init__(self)
        self.what = _("Got unexpected data from the daemon (is the database properly updated?).")

    def __str__(self):
        return self.what
