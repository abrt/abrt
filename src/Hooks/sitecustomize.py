# This special script is placed in
# /usr/local/lib/pythonNNN/site-packages/sitecustomize.py
# and python interpreter runs it automatically everytime
# some python script is executed

config = None
conf = {}
try:
    config = open("/etc/abrt/pyhook.conf","r")
except:
    #silently ignore if file doesn't exist
    pass

try:
    if config:
        # we expect config in form
        # key = value
        # Enabled = yes
        # this should strip
        line = config.readline().lower().replace(' ','').strip('\n').split('=')
        conf[line[0]] = line[1]
except:
    # ignore silently everything, because we don't want to bother user if this hook doesn't work
    pass

if conf.has_key("enabled"):
    if conf["enabled"] == "yes":
        try:
            from abrt_exception_handler import *

            installExceptionHandler(debug = 1)
        except Exception, e:
            # FIXME don't print anything, write it to some log file
            print e
            pass
