#! /usr/bin/env python
import sys,os
class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value

class Expand(object):
  @staticmethod
  def at():
    if (len(sys.argv) < 2):
      return []
    return  sys.argv[1:]
  @staticmethod
  def star(in_quotes):
    if (in_quotes):
      if (len(sys.argv) < 2):
        return ""
      return " ".join(sys.argv[1:])
    return Expand.at()

# This is bashtime.sh
# Copyright (c) 2013 Paul Scott-Murphy
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# if no args
if (Expand.star(1) == '' ):
    # get the date as "hours(12) minutes" in a single call
    # make a bash array with it
    time=Bash2Py(os.popen("date \"+%I%M\"").read().rstrip("\n"))
else:
    # get the arguments passed to the script
    hour=Bash2Py(sys.argv[1])
    min=Bash2Py(sys.argv[2])
    time=Bash2Py(str(hour.val)+str(min.val))
if (int(time.val) < 115 ):
    print("\xf0\x9f\x95\x90",end="")
elif (int(time.val) < 145 ):
    print("\xf0\x9f\x95\x9c",end="")
elif (int(time.val) < 215 ):
    print("\xf0\x9f\x95\x91",end="")
elif (int(time.val) < 245 ):
    print("\xf0\x9f\x95\x9d",end="")
elif (int(time.val) < 315 ):
    print("\xf0\x9f\x95\x92",end="")
elif (int(time.val) < 345 ):
    print("\xf0\x9f\x95\x9e",end="")
elif (int(time.val) < 415 ):
    print("\xf0\x9f\x95\x93",end="")
elif (int(time.val) < 445 ):
    print("\xf0\x9f\x95\x9f",end="")
elif (int(time.val) < 515 ):
    print("\xf0\x9f\x95\x94",end="")
elif (int(time.val) < 545 ):
    print("\xf0\x9f\x95\xa0",end="")
elif (int(time.val) < 615 ):
    print("\xf0\x9f\x95\x95",end="")
elif (int(time.val) < 645 ):
    print("\xf0\x9f\x95\xa1",end="")
elif (int(time.val) < 715 ):
    print("\xf0\x9f\x95\x96",end="")
elif (int(time.val) < 745 ):
    print("\xf0\x9f\x95\xa2",end="")
elif (int(time.val) < 815 ):
    print("\xf0\x9f\x95\x97",end="")
elif (int(time.val) < 845 ):
    print("\xf0\x9f\x95\xa3",end="")
elif (int(time.val) < 915 ):
    print("\xf0\x9f\x95\x98",end="")
elif (int(time.val) < 945 ):
    print("\xf0\x9f\x95\xa4",end="")
elif (int(time.val) < 1015 ):
    print("\xf0\x9f\x95\x99",end="")
elif (int(time.val) < 1045 ):
    print("\xf0\x9f\x95\xa5",end="")
elif (int(time.val) < 1115 ):
    print("\xf0\x9f\x95\x9a",end="")
elif (int(time.val) < 1145 ):
    print("\xf0\x9f\x95\xa6",end="")
elif (int(time.val) < 1215 ):
    print("\xf0\x9f\x95\x9b",end="")
elif (int(time.val) < 1300 ):
    print("\xf0\x9f\x95\x9b",end="")
else:
    print("\xe2\xad\x95",end="")
