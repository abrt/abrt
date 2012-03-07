#! /usr/bin/python
import CCDBusBackend

dm = CCDBusBackend.DBusManager()
for a in dm.daemon().GetProblems():
    print a
print dm.daemon().GetInfo("ahoj")
print dm.daemon().GetAllProblems()
#print dm.daemon().Quit()
