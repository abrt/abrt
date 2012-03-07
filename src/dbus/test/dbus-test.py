#! /usr/bin/python
import CCDBusBackend

dm = CCDBusBackend.DBusManager()
for a in dm.daemon().GetProblems():
    print a
print dm.daemon().GetInfo("ahoj")
for a in dm.daemon().GetAllProblems():
    print a
#print dm.daemon().Quit()
