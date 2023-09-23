#! /usr/bin/env python
from __future__ import print_function
class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value

name=Bash2Py("john")
how_many=Bash2Py(2)
cost=Bash2Py("10.5")
print( "%s purchased %d apples, which cost him %f dollars.\n" % (str(name.val), str(how_many.val), str(cost.val)) )

