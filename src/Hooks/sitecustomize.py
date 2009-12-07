# ABRT crash hook
#
# This special script is placed in
# /usr/local/lib/pythonNNN/site-packages/sitecustomize.py
# and python interpreter runs it automatically everytime
# some python script is executed.

def abrt_daemon_ok():
    try:
        #FIXME: make it relocable! this will work only when installed in default path
        #pidfile = open(VAR_RUN_PID_FILE, "r");
        pidfile = open("/var/run/abrt.pid", "r")
    except Exception, ex:
        # log the exception?
        return False
        
    pid = pidfile.readline()
    pidfile.close()
    if not pid:
        return False
    
    try:
        # pid[:-1] strips the trailing '\n'
        cmdline = open("/proc/%s/cmdline" % pid[:-1], "r").readline()
    except Exception, ex:
        # can't read cmdline
        return False
    if not ("abrtd" in cmdline):
        return False
        
    return True

if abrt_daemon_ok():
    # Prevent abrt exception handler from running when the abrtd daemon is
    # not active.
    try:
        from abrt_exception_handler import installExceptionHandler

        installExceptionHandler(debug = 1)
    except Exception, e:
        # FIXME: log errors?
        pass
else:
    #FIXME: log something?
    pass
