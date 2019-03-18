#! /usr/bin/env python
import sys,os,subprocess
class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value

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

def Str(value):
  if isinstance(value, list):
    return " ".join(value)
  return value

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
  def hash():
    return  len(sys.argv)-1
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

def throw () :
    print(Expand.star(1),stderr=subprocess.STDOUT)
    exit(1)

BRIEF=Bash2Py(0)
LEAFONLY=Bash2Py(0)
PRUNE=Bash2Py(0)
def usage () :
    print()
    print("Usage: JSON.sh [-b] [-l] [-p] [-h]")
    print()
    print("-p - Prune empty. Exclude fields with empty values.")
    print("-l - Leaf only. Only show leaf nodes, which stops data duplication.")
    print("-b - Brief. Combines 'Leaf only' and 'Prune empty' options.")
    print("-h - This help text.")
    print()

def parse_options (_p1) :
    global BRIEF
    global LEAFONLY
    global PRUNE

    subprocess.call(["set","--",Str(Expand.at())],shell=True)
    ARGN=Bash2Py(Expand.hash())
    while (int(ARGN.val) != 0):
        
        if ( str(_p1) == '-h'):
            usage()
            exit(0)
        elif ( str(_p1) == '-b'):
            BRIEF=Bash2Py(1)
            LEAFONLY=Bash2Py(1)
            PRUNE=Bash2Py(1)
        elif ( str(_p1) == '-l'):
            LEAFONLY=Bash2Py(1)
        elif ( str(_p1) == '-p'):
            PRUNE=Bash2Py(1)
        elif ( str(_p1) == '?*'):
            print("ERROR: Unknown option.")
            usage()
            exit(0)
        subprocess.call(["shift","1"],shell=True)
        ARGN=Bash2Py((ARGN.val - 1))

def awk_egrep (_p1) :

    pattern_string=Bash2Py(_p1)
    _rc0 = subprocess.call(["gawk","{\n    while ($0) {\n      start=match($0, pattern);\n      token=substr($0, start, RLENGTH);\n      print token;\n      $0=substr($0, start+RLENGTH);\n    }\n  }","pattern="+str(pattern_string.val)],shell=True)

def tokenize () :
    global GREP
    global ESCAPE
    global CHAR

    "GREP"
    "ESCAPE"
    "CHAR"
    if (_rcr8, _rcw8 = os.pipe()
    if os.fork():
        os.close(_rcw8)
        os.dup2(_rcr8, 0)
        subprocess.call("egrep" + " " + "-ao" + " " + "--color=never" + " " + "test",shell=True,stderr=subprocess.STDOUT,stdout=file('/dev/null','wb'))
        
    else:
        os.close(_rcr8)
        os.dup2(_rcw8, 1)
        print("test string")
        sys.exit(0) ):
        GREP=Bash2Py("egrep -ao --color=never")
    else:
        GREP=Bash2Py("egrep -ao")
    if (_rcr7, _rcw7 = os.pipe()
    if os.fork():
        os.close(_rcw7)
        os.dup2(_rcr7, 0)
        subprocess.call("egrep" + " " + "-o" + " " + "test",shell=True,stderr=subprocess.STDOUT,stdout=file('/dev/null','wb'))
        
    else:
        os.close(_rcr7)
        os.dup2(_rcw7, 1)
        print("test string")
        sys.exit(0) ):
        ESCAPE=Bash2Py("(\\\\[^u[:cntrl:]]|\\\\u[0-9a-fA-F]{4})")
        CHAR=Bash2Py("[^[:cntrl:]\"\\\\]")
    else:
        GREP=Bash2Py("awk_egrep")
        ESCAPE=Bash2Py("(\\\\\\\\[^u[:cntrl:]]|\\\\u[0-9a-fA-F]{4})")
        CHAR=Bash2Py("[^[:cntrl:]\"\\\\\\\\]")
    STRING=Bash2Py("\""+str(CHAR.val)+"*("+str(ESCAPE.val)+str(CHAR.val)+"*)*\"")
    NUMBER=Bash2Py("-?(0|[1-9][0-9]*)([.][0-9]*)?([eE][+-]?[0-9]*)?")
    KEYWORD=Bash2Py("null|false|true")
    SPACE=Bash2Py("[[:space:]]+")
    _rc0 = _rcr2, _rcw2 = os.pipe()
    if os.fork():
        os.close(_rcw2)
        os.dup2(_rcr2, 0)
        subprocess.call(["egrep","-v","^"+str(SPACE.val)+"$"],shell=True)
    else:
        os.close(_rcr2)
        os.dup2(_rcw2, 1)
        subprocess.call([str(GREP.val),str(STRING.val)+"|"+str(NUMBER.val)+"|"+str(KEYWORD.val)+"|"+str(SPACE.val)+"|."],shell=True)
        sys.exit(0)
    

def parse_array (_p1) :
    global token
    global value
    global BRIEF

    index=Bash2Py(0)
    ary=Bash2Py()
    token = Bash2Py(raw_input())
    
    if ( str(token.val) == ']'):
    
    else:
        while (pass):
            subprocess.call(["parse_value",str(_p1),str(index.val)],shell=True)
            index=Bash2Py((index.val + 1))
            ary=Bash2Py(str(ary.val)+str(value.val))
            token = Bash2Py(raw_input())
            
            elif ( str(token.val) == ']'):
                break
            elif ( str(token.val) == ','):
                ary=Bash2Py(str(ary.val)+",")
            else:
                throw("EXPECTED , or ] GOT "+Expand.colonMinus("token","EOF"))
            token = Bash2Py(raw_input())
    _rc0 = ((int(BRIEF.val) == 0) and (value=Bash2Py(os.popen("printf \"[%s]\" \""+str(ary.val)+"\"").read().rstrip("\n")))) or (value=Bash2Py())
    pass

def parse_object (_p1) :
    global token
    global key
    global value
    global BRIEF

    "key"
    obj=Bash2Py()
    token = Bash2Py(raw_input())
    
    if ( str(token.val) == '}'):
    
    else:
        while (pass):
            
            elif ( str(token.val) == ''"'*'"''):
                key=Bash2Py(token.val)
            else:
                throw("EXPECTED string GOT "+Expand.colonMinus("token","EOF"))
            token = Bash2Py(raw_input())
            
            if ( str(token.val) == ':'):
            
            else:
                throw("EXPECTED : GOT "+Expand.colonMinus("token","EOF"))
            token = Bash2Py(raw_input())
            subprocess.call(["parse_value",str(_p1),str(key.val)],shell=True)
            obj=Bash2Py(str(obj.val)+str(key.val)+":"+str(value.val))
            token = Bash2Py(raw_input())
            
            if ( str(token.val) == '}'):
                break
            elif ( str(token.val) == ','):
                obj=Bash2Py(str(obj.val)+",")
            else:
                throw("EXPECTED , or } GOT "+Expand.colonMinus("token","EOF"))
            token = Bash2Py(raw_input())
    _rc0 = ((int(BRIEF.val) == 0) and (value=Bash2Py(os.popen("printf \"{%s}\" \""+str(obj.val)+"\"").read().rstrip("\n")))) or (value=Bash2Py())
    pass

def parse_value (_p1,_p2) :
    global token
    global value
    global LEAFONLY
    global PRUNE

    jpath=Bash2Py(Expand.colonPlus("1",str(_p1)+",")+str(_p2))
    isleaf=Bash2Py(0)
    isempty=Bash2Py(0)
    print=Bash2Py(0)
    
    if ( str(token.val) == '{'):
        parse_object(jpath.val)
    elif ( str(token.val) == '['):
        parse_array(jpath.val)
    elif ( str(token.val) == '' or str(token.val) == '[!0-9]'):
        # At this point, the only valid single-character tokens are digits.
        throw("EXPECTED value GOT "+Expand.colonMinus("token","EOF"))
    else:
        value=Bash2Py(token.val)
        isleaf=Bash2Py(1)
        (value.val  "=""\"\"" != '') and (isempty=Bash2Py(1))
    _rc0 = (value.val  "="str() != '') and (return)
    _rc0 = ((int(LEAFONLY.val) == 0) and (int(PRUNE.val) == 0)) and (print=Bash2Py(1))
    _rc0 = (((int(LEAFONLY.val) == 1) and (int(isleaf.val) == 1)) and (int(PRUNE.val) == 0)) and (print=Bash2Py(1))
    _rc0 = (((int(LEAFONLY.val) == 0) and (int(PRUNE.val) == 1)) and (int(isempty.val) == 0)) and (print=Bash2Py(1))
    _rc0 = ((((int(LEAFONLY.val) == 1) and (int(isleaf.val) == 1)) and (int(PRUNE.val) == 1)) and (int(isempty.val) == 0)) and (print=Bash2Py(1))
    _rc0 = (int(print.val) == 1) and (print( "[%s]\t%s\n" % (str(jpath.val), str(value.val)) )
    )
    pass

def parse () :
    global token

    token = Bash2Py(raw_input())
    parse_value()
    token = Bash2Py(raw_input())
    
    if ( str(token.val) == ''):
    
    else:
        throw("EXPECTED EOF GOT "+str(token.val))

parse_options(Expand.at())
if (((__file__  "="str(BASH_SOURCE.val) != '') or ( not (str(BASH_SOURCE.val) != ''))) ):
    _rcr1, _rcw1 = os.pipe()
    if os.fork():
        os.close(_rcw1)
        os.dup2(_rcr1, 0)
        parse()
    else:
        os.close(_rcr1)
        os.dup2(_rcw1, 1)
        tokenize()
        sys.exit(0)

