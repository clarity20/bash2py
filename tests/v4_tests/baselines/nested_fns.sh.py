#! /usr/bin/env python
from __future__ import print_function
class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value
  def setValue(self, value=None):
    self.val = value
    return value

def GetVariable(name, local=locals()):
  if name in local:
    return local[name]
  if name in globals():
    return globals()[name]
  return None

def Make(name, local=locals()):
  ret = GetVariable(name, local)
  if ret is None:
    ret = Bash2Py(0)
    globals()[name] = ret
  return ret

    Make("y").setValue("yyyyy")
    print("in f2, y is",y.val)
) :
    Make("x").setValue("xxxxx")
    print("in f1, x is",x.val)
    def f2 () :
        global x
        global y
    
        Make("y").setValue("yyyyy")
        print("in f2, y is",y.val)
    
    f2()

f1()
