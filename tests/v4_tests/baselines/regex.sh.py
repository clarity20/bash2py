#! /usr/bin/env python
from __future__ import print_function
import sys,re
class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value

class Expand(object):
  @staticmethod
  def hash():
    return  len(sys.argv)-1

if ((Expand.hash().val < 1) ):
    print("Usage:",__file__,"word")
    exit(0)
match_object = re.search(r"r(.)",str(sys.argv[1]))
if (match_object):
    print(sys.argv[1],"contains r plus",str(match_object.group(1))+".")
match_object = re.search(r"x",str(sys.argv[1]))
if (match_object):
    print(sys.argv[1],"contains x.")
else:
    match_object = re.search(r"w",str(sys.argv[1]))
    if (match_object):
        print(sys.argv[1],"contains w.")
print("To reiterate (x>w>r.),",sys.argv[1],"contains",str(match_object.group(0))+".")
match_object = re.search(r"r(.)",str(sys.argv[1]))
print("Successor of \'r\' in",sys.argv[1],"is",str(match_object.group(1))+".")
