# ABRT crash hook
#
# This special script is placed in
# /usr/local/lib/pythonNNN/site-packages/sitecustomize.py
# and python interpreter runs it automatically everytime
# some python script is executed.

try:
    from abrt_exception_handler import installExceptionHandler
    installExceptionHandler()
except Exception, e:
    # TODO: log errors?
    # OTOH, if abrt is deinstalled uncleanly
    # and this file (sitecustomize.py) exists but
    # abrt_exception_handler module does not exist, we probably
    # don't want to irritate admins...
    pass
