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
plain=Bash2Py("(🕐 🕑 🕒 🕓 🕔 🕕 🕖 🕗 🕘 🕙 🕚 🕛)")
half=Bash2Py("(🕜 🕝 🕞 🕟 🕠 🕡 🕢 🕣 🕤 🕥 🕦 🕧)")
# array index starts at 0
hi=Bash2Py((hour.val - 1))
if (int(min.val) < 15 ):
    print(plain.val[hi.val] ],end="")
elif (int(min.val) < 45 ):
    print(half.val[hi.val] ],end="")
else:
    print(plain.val[(hi.val + 1)] ],end="")
