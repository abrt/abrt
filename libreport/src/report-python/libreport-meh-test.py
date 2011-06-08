#!/bin/env python

from meh.dump import ReverseExceptionDump
from meh.handler import *
from meh.ui.gui import *

class Config:
      def __init__(self):
          self.programName = "abrt"
          self.programVersion = "2.0"
          self.attrSkipList = []
          self.fileList = []
          self.config_value_one = 1
          self.config_value_two = 2



#meh.makeRHHandler("crash-test-meh", "1.0", Config())
config = Config()
intf = GraphicalIntf(None)
handler = ExceptionHandler(config, intf, ReverseExceptionDump)
handler.install(None)


print "handler set up, about to divide by zero"

zero = 0
print 1 / zero

print "should have crashed"

