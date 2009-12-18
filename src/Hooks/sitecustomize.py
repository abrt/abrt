# ABRT crash hook
#
# This special script is placed in
# /usr/local/lib/pythonNNN/site-packages/sitecustomize.py
# and python interpreter runs it automatically everytime
# some python script is executed.

try:
    from abrt_exception_handler import installExceptionHandler
    installExceptionHandler(debug = 1)
except Exception, e:
    # FIXME: log errors?
    pass
