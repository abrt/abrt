#! /usr/bin/python
import CCDBusBackend
import sys

dm = CCDBusBackend.DBusManager()

for a in dm.daemon().GetProblems("/var/spool/abrt"):
    print a
print "GetInfo:\n\t", dm.daemon().GetInfo("/var/spool/abrt/pyhook-2012-02-10-15:36:10-19157")
#print ">>> AUTHORIZED <<<"
for a in dm.daemon().GetAllProblems("/var/spool/abrt"):
    print a
#print dm.daemon().Quit()
#print ">>>> asking again, now it shouldn't ask for password"
#for a in dm.daemon().GetAllProblems():
#    print a

#try:    
#    dm.daemon().ChownProblemDir("/var/spool/abrt/ccpp-2012-02-08-23:03:29-1113")
#    print "Access allowed, running wizard..."
    # run
    # $ report-gtk -d problem_dir
#except Exception, ex:
#    print "Access not allowed...", str(ex)
