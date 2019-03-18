#! /usr/bin/env python
import sys,os,subprocess,signal
class Bash2PyException(Exception):
  def __init__(self, value=None):
    self.value = value
  def __str__(self):
    return repr(self.value)

class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value
  def postinc(self,inc=1):
    tmp = self.val
    self.val += inc
    return tmp

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
  def exclamation():
    raise Bash2PyException("$! unsupported")
  @staticmethod
  def underbar():
    raise Bash2PyException("$_ unsupported")
  @staticmethod
  def colonMinus(name, value=''):
    ret = GetValue(name)
    if (ret is None or ret == ''):
		ret = value
    return ret
  @staticmethod
  def colonPlus(name, value=''):
    ret = GetValue(name)
    if (ret is None or ret == ''):
      return ''
    return value

# assert.sh 1.0 - bash unit testing framework
# Copyright (C) 2009, 2010, 2011, 2012 Robert Lehmann
#
# http://github.com/lehmannro/assert.sh
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
os.environ['DISCOVERONLY'] = Expand.colonMinus("DISCOVERONLY")
os.environ['DEBUG'] = Expand.colonMinus("DEBUG")
os.environ['STOP'] = Expand.colonMinus("STOP")
os.environ['INVARIANT'] = Expand.colonMinus("INVARIANT")
os.environ['CONTINUE'] = Expand.colonMinus("CONTINUE")
_rc0 = (args=Bash2Py(os.popen("getopt -n \""+__file__+"\" -l     verbose,help,stop,discover,invariant,continue vhxdic "+Expand.star(1)).read().rstrip("\n"))) or (exit(-1))
arg=Bash2Py(0)
for arg.val in [args.val]:
    
    if ( str(arg.val) == '-h'):
        print(__file__+" [-vxidc]","[--verbose] [--stop] [--invariant] [--discover] [--continue]")
        print(os.popen("sed 's/./ /g' <<< \""+__file__+"\"").read().rstrip("\n")+" [-h] [--help]")
        exit(0)
    elif ( str(arg.val) == '--help'):
        subprocess.Popen("cat",shell=True,stdin=subprocess.PIPE)
        _rc0.communicate("Usage: "+__file__+" [options]\nLanguage-agnostic unit tests for subprocesses.\n\nOptions:\n  -v, --verbose    generate output for every individual test case\n  -x, --stop       stop running tests after the first failure\n  -i, --invariant  do not measure timings to remain invariant between runs\n  -d, --discover   collect test suites only, do not run any tests\n  -c, --continue   do not modify exit code to test suite status\n  -h               show brief usage information and exit\n  --help           show this help message and exit\n")
        _rc0 = _rc0.wait()
        exit(0)
    elif ( str(arg.val) == '-v' or str(arg.val) == '--verbose'):
        DEBUG=Bash2Py(1)
    elif ( str(arg.val) == '-x' or str(arg.val) == '--stop'):
        STOP=Bash2Py(1)
    elif ( str(arg.val) == '-i' or str(arg.val) == '--invariant'):
        INVARIANT=Bash2Py(1)
    elif ( str(arg.val) == '-d' or str(arg.val) == '--discover'):
        DISCOVERONLY=Bash2Py(1)
    elif ( str(arg.val) == '-c' or str(arg.val) == '--continue'):
        CONTINUE=Bash2Py(1)
_indent = "\n\t"
# local format helper
def _assert_reset () :
    global tests_ran
    global tests_failed
    global tests_errors
    global tests_starttime

    tests_ran=Bash2Py(0)
    tests_failed=Bash2Py(0)
    tests_errors=Bash2Py("()")
    tests_starttime=Bash2Py(os.popen("date +%s.%N").read().rstrip("\n"))

# seconds_since_epoch.nanoseconds
def assert_end () :
    global tests_endtime
    global tests
    global tests_ran
    global DISCOVERONLY
    global DEBUG
    global INVARIANT
    global report_time
    global tests_starttime
    global tests_failed
    global tests_errors
    global error
    global tests_failed_previous
    global tests_suite_status

    # assert_end [suite ..]
    tests_endtime=Bash2Py(os.popen("date +%s.%N").read().rstrip("\n"))
    tests=Bash2Py(str(tests_ran.val)+" "+Expand.colonPlus("*",Expand.star(1)+" ")+"tests")
    _rc0 = (((str(DISCOVERONLY.val) != '') and (print("collected "+str(tests.val)+"."))) and (_assert_reset())) and (return)
    _rc0 = (str(DEBUG.val) != '') and (print())
    _rc0 = ((str(INVARIANT.val) == '') and (report_time=Bash2Py(" in "+os.popen("bc         <<< \""+str(tests_endtime.val%.N)+" - "+str(tests_starttime.val%.N)+"\"         | sed -e 's/.([0-9]{0,3})[0-9]*/.1/' -e 's/^./0./'").read().rstrip("\n")+"s"))) or (report_time=Bash2Py())
    if (int(tests_failed.val) == 0 ):
        print("all "+str(tests.val)+" passed"+str(report_time.val)+".")
    else:
        error=Bash2Py(0)
        for error.val in [tests_errors.val[@] ]]:
            print(error.val)
        print(str(tests_failed.val)+" of "+str(tests.val)+" failed"+str(report_time.val)+".")
    tests_failed_previous=Bash2Py(tests_failed.val)
    _rc0 = (int(tests_failed.val) > 0) and (tests_suite_status=Bash2Py(1))
    _assert_reset()
    return(int(tests_failed_previous.val))

def assert (_p1,_p2,_p3) :
    global DISCOVERONLY
    global result
    global expected
    global DEBUG

    # assert <command> <expected stdout> [stdin]
    (Make("tests_ran").postinc()) or (pass)
    _rc0 = ((str(DISCOVERONLY.val) != '') and (return)) or (True)
    # printf required for formatting
    expected = "x"+Expand.colonMinus("2")
    # x required to overwrite older results
    _rc0 = (result=Bash2Py(os.popen("eval 2>/dev/null "+str(_p1)+" <<< "+Expand.colonMinus("3")).read().rstrip("\n"))) or (True)
    # Note: $expected is already decorated
    if ("x"+str(result.val) == str(expected.val) ):
        ((str(DEBUG.val) != '') and (print(".",end=""))) or (True)
        return
    result=Bash2Py(os.popen("sed -e :a -e '"+str(Expand.exclamation())+"N;s/n/\\n/;ta' <<< \""+str(result.val)+"\"").read().rstrip("\n"))
    _rc0 = ((str(result.val) == '') and (result=Bash2Py("nothing"))) or (result=Bash2Py("\""+str(result.val)+"\""))
    _rc0 = ((str(_p2) == '') and (expected=Bash2Py("nothing"))) or (expected=Bash2Py("\""+str(_p2)+"\""))
    _rc0 = subprocess.call(["_assert_fail","expected "+str(expected.val)+Expand.underbar()indent+"got "+str(result.val),str(_p1),str(_p3)],shell=True)

def assert_raises (_p1,_p2,_p3) :
    global DISCOVERONLY
    global status
    global expected
    global DEBUG

    # assert_raises <command> <expected code> [stdin]
    (Make("tests_ran").postinc()) or (pass)
    _rc0 = ((str(DISCOVERONLY.val) != '') and (return)) or (True)
    status=Bash2Py(0)
    _rc0 = (> /dev/null2>&1(eval(str(_p1))) > /dev/null 2>&1) or (status=Bash2Py(_rc0))
    expected=Bash2Py(Expand.colonMinus("2","0"))
    if (int(status.val) == int(expected.val) ):
        ((str(DEBUG.val) != '') and (print(".",end=""))) or (True)
        return
    _rc0 = subprocess.call(["_assert_fail","program terminated with code "+str(status.val)+" instead of "+str(expected.val),str(_p1),str(_p3)],shell=True)

def _assert_fail (_p1,_p2,_p3) :
    global DEBUG
    global report
    global tests_ran
    global STOP
    global tests_errors
    global tests_failed

    # _assert_fail <failure> <command> <stdin>
    (str(DEBUG.val) != '') and (print("X",end=""))
    report=Bash2Py("test #"+str(tests_ran.val)+" \""+str(_p2)+Expand.colonPlus("3"," <<< "+str(_p3))+"\" failed:"+Expand.underbar()indent+str(_p1))
    if (str(STOP.val) != '' ):
        (str(DEBUG.val) != '') and (print())
        print(report.val)
        exit(1)
    tests_errors[tests_failed]=Bash2Py(report.val)
    _rc0 = (Make("tests_failed").postinc()) or (pass)

_assert_reset()
pass
# remember if any of the tests failed so far
def _assert_cleanup () :
    global CONTINUE
    global tests_suite_status

    status=Bash2Py(_rc0)
    # modify exit code if it's not already non-zero
    _rc0 = (int(status.val) == 0 and str(CONTINUE.val) == '') and (exit(int(tests_suite_status.val)))

signal.signal(signal.SIGEXIT,_assert_cleanup)

