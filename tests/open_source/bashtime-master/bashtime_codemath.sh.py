#! /usr/bin/env python
import sys,os
class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value

def IsDefined(name, local=locals()):
  return name in globals() or name in local

def GetVariable(name, local=locals()):
  if name in local:
    return local[name]
  if name in globals():
    return globals()[name]
  return None

def GetValue(name, local=locals()):
  variable = GetVariable(name,local)
  if variable is None or variable.val is None:
    return ''
  return variable.val

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
  @staticmethod
  def minus(name, value=''):
    if (IsDefined(name)):
      return GetValue(name)
    return value

# This is bashtime.sh
# Copyright (c) 2013 Paul Scott-Murphy (idea, initial code)
# Copyright (c) 2013 Johann Dreo (new code)
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
    d=Bash2Py("("+os.popen("date \"+%I %M\"").read().rstrip("\n")+")")
    # separate hours and minutes
    hour=Bash2Py(d.val[0] ]#0)
    # remove leading 0 or values <10 will be interpreted as octal
    min=Bash2Py(d.val[1] ]#0)
else:
    # get the arguments passed to the script
    hour=Bash2Py(sys.argv[1]#0)
    min=Bash2Py(sys.argv[2]#0)
# The targeted unicode characters are the "CLOCK FACE" ones
# They are located in the codepages between:
#     U+1F550 (ONE OCLOCK) and U+1F55B (TWELVE OCLOCK), for the plain hours
#     U+1F55C (ONE-THIRTY) and U+1F567 (TWELVE-THIRTY), for the thirties
#
# Those codes may be output with unicode escapes or hexadecimal escapes,
# the later being the more portable.
#
# But we can iterate only over integers.
#
# We thus need the following conversion table:
#       utf   hex   int
# hours 50:5B 90:9b 144:155
# half  5C:67 9c:a7 156:167
# The characters being grouped bas plain/thirty, we must first now
# if we are close to the 0 or 30 minutes.
# Bash using integer arithmetic by default, we do not need rounding.
# We thus add 0 (plain hour) or 12 (half).
# Then we add 144, which is the first index (as an integer).
# (0 … 16 … 31 … 46 …) -> (0 1 2 3)
a=Bash2Py(os.popen("echo "+str((min.val // 15))).read().rstrip("\n"))
# (0 1 2 3) -> (0 1 1 2)
i=Bash2Py(os.popen("echo "+str(a.val)+" | awk \"{ print $1/1.5 }\" OFMT=\"%.0f\"").read().rstrip("\n"))
# (0 1 1 2) -> (0 12 12 -1)
j=Bash2Py(((((i.val * 12) + 1) % 25) - 1))
# (0 12 12 -1) -> (0 12 12 1)
k=Bash2Py(Expand.minus("j#"))
# start from the first code
mi=Bash2Py((144 + k.val))
# Add the computed minutes index (144 or 156) minus 1 (because the first hour starts at 0).
hi=Bash2Py((mi.val + (hour.val - 1)))
# Get the hexadecimal representation of this integer
hex=Bash2Py(os.popen("printf \"%x\" "+str(hi.val)).read().rstrip("\n"))
# Print the first three bytes (that are always the same) and the computed last one.
print( "\xf0\x9f\x95\x"+str(hex.val) )

