#! /usr/bin/env python
import sys,os,subprocess,signal
class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value
  def setValue(self, value=None):
    self.val = value
    return value
  def postinc(self,inc=1):
    tmp = self.val
    self.val += inc
    return tmp
  def plus(value):
    self.val += value
    return self.val
  def minus(value):
    self.val -= value
    return self.val

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

class Expand(object):
  @staticmethod
  def hash():
    return  len(sys.argv)-1

#important variables
board=""

# array that keeps track of game status
pieces=0

# number of pieces present on board
"score=0"

# score variable
flag_skip=0

# flag that prevents doing more than one operation on
# single field in one step
moves=0

# stores number of possible moves to determine if player lost 
# the game
"ESC=\x1b"

# escape byte
"header=Bash 2048 v1.1 (https://github.com/mydzor/bash2048)"

#default config
"board_size=4"

"target=2048"

#for colorizing numbers
colors=""

colors[2]=Bash2Py(33)
# yellow text
colors[4]=Bash2Py(32)
# green text
colors[8]=Bash2Py(34)
# blue text
colors[16]=Bash2Py(36)
# cyan text
colors[32]=Bash2Py(35)
# purple text
colors[64]=Bash2Py("33m\033[7")
# yellow background
colors[128]=Bash2Py("32m\033[7")
# green background
colors[256]=Bash2Py("34m\033[7")
# blue background
colors[512]=Bash2Py("36m\033[7")
# cyan background
colors[1024]=Bash2Py("35m\033[7")
# purple background
colors[2048]=Bash2Py("31m\033[7")
# red background (won with default target)
_rc0 = subprocess.call("exec",shell=True,std3=file('/dev/null','wb'))

# no logging by default
signal.signal(signal.SIGINT,"end_game 0")

#handle INT signal
#simplified replacement of seq command
def _seq (_p1,_p2,_p3) :
    global max

    cur=Bash2Py(1)
    "max"
    inc=Bash2Py(1)
    
    if ( str(Expand.hash()) == '1'):
        Make("max").setValue(_p1.val)
    elif ( str(Expand.hash()) == '2'):
        Make("cur").setValue(_p1.val)
        Make("max").setValue(_p2.val)
    elif ( str(Expand.hash()) == '3'):
        Make("cur").setValue(_p1.val)
        Make("inc").setValue(_p2.val)
        Make("max").setValue(_p3.val)
    while (int(max.val) >= int(cur.val)):
        print( str(cur.val)+" " )
        
        Make("cur").plus(inc.val)

# print currect status of the game, last added pieces are marked red
def print_board () :
    global header
    global pieces
    global target
    global score
    global index_max
    global board
    global board_size
    global colors

    subprocess.call(["clear"],shell=True)
    print( str(header.val)+" pieces="+str(pieces.val)+" target="+str(target.val)+" score="+str(score.val)+"\n" )
    
    print( "Board status:\n" )
    
    print( "\n" )
    
    print( "/------" )
    
    l=Bash2Py(0)
    for l.val in [os.popen("_seq 1 "+str(index_max.val)).read().rstrip("\n")]:
        print( "|------" )
    
    print( "\\\\\\n" )
    
    l=Bash2Py(0)
    for l.val in [os.popen("_seq 0 "+str(index_max.val)).read().rstrip("\n")]:
        print( "|" )
        
        m=Bash2Py(0)
        for m.val in [os.popen("_seq 0 "+str(index_max.val)).read().rstrip("\n")]:
            if (board[l*board_size+m] ] ):
                if (((last_added.val == ((l.val * board_size.val) + m.val)) | (first_round.val == ((l.val * board_size.val) + m.val))) ):
                    print( "\\033[1m\\033[31m %4d \\033[0m|" % (str(board.val[l*str(board_size.val)+m] ])) )
                
                else:
                    print( "\033[1m\033["+str(colors.val[str(board.val[l*str(board_size.val)+m] ])] ])+"m %4d\033[0m |" % (str(board.val[l*str(board_size.val)+m] ])) )
                
                print( " %4d |" % (str(board.val[l*str(board_size.val)+m] ])) )
            
            else:
                print( "      |" )
                
                print( "      |" )
        
        ((l.val == index_max.val)) or (
            print( "\\n|------" )
            
            l=Bash2Py(0)
            for l.val in [os.popen("_seq 1 "+str(index_max.val)).read().rstrip("\n")]:
                print( "|------" )
            
            print( "|\\n" )
            
            print( "\\n" )
            
        )
    print( "\\n\\\\------" )
    
    l=Bash2Py(0)
    for l.val in [os.popen("_seq 1 "+str(index_max.val)).read().rstrip("\n")]:
        print( "|------" )
    
    print( "/\\n" )
    

# Generate new piece on the board
# inputs:
#         $board  - original state of the game board
#         $pieces - original number of pieces
# outputs:
#         $board  - new state of the game board
#         $pieces - new number of pieces
def generate_piece () :
    global pos
    global board
    global value
    global last_added

    while (True):
        Make("pos").setValue((RANDOM.val % fields_total.val))
        (board) or (
            Make("value").setValue((2 if (RANDOM.val % 10) else 4))
            board[pos]=Bash2Py(value.val)
            last_added=Bash2Py(pos.val)
            print( "Generated new piece with value "+str(value.val)+" at position ["+str(pos.val)+"]\n" )
            
            break
        )
    _rc0= not Make("pieces").postinc()
    _rc0 = Bash2Py(_rc0)

# perform push operation between two pieces
# inputs:
#         $1 - push position, for horizontal push this is row, for vertical column
#         $2 - recipient piece, this will hold result if moving or joining
#         $3 - originator piece, after moving or joining this will be left empty
#         $4 - direction of push, can be either "up", "down", "left" or "right"
#         $5 - if anything is passed, do not perform the push, only update number 
#              of valid moves
#         $board - original state of the game board
# outputs:
#         $change    - indicates if the board was changed this round
#         $flag_skip - indicates that recipient piece cannot be modified further
#         $board     - new state of the game board
def push_pieces (_p1,_p2,_p3,_p4,_p5) :
    global board_size
    global board
    global first
    global second
    global target

    
    if ( str(_p4) == 'up'):
        Make("first").setValue(((_p2.val * board_size.val) + _p1.val))
        Make("second").setValue((((_p2.val + _p3.val) * board_size.val) + _p1.val))
    elif ( str(_p4) == 'down'):
        Make("first").setValue((((index_max.val - _p2.val) * board_size.val) + _p1.val))
        Make("second").setValue((((index_max.val - (_p2.val - _p3.val)) * board_size.val) + _p1.val))
    elif ( str(_p4) == 'left'):
        Make("first").setValue(((_p1.val * board_size.val) + _p2.val))
        Make("second").setValue(((_p1.val * board_size.val) + (_p2.val + _p3.val)))
    elif ( str(_p4) == 'right'):
        Make("first").setValue(((_p1.val * board_size.val) + (index_max.val - _p2.val)))
        Make("second").setValue(((_p1.val * board_size.val) + (index_max.val - (_p2.val - _p3.val))))
    _rc0 = (board[first] ]) or (
        (board[second] ]) and (
            if (str(_p5) == '' ):
                board[first]=Bash2Py(board.val[second.val] ])
                board
                Make("change").setValue(1)
                print( "move piece with value "+str(board.val[str(first.val)] ])+" from ["+str(second.val)+"] to ["+str(first.val)+"]\n" )
            
            else:
                Make("moves").postinc()
            return
        )
        return
    )
    _rc0 = (board[second] ]) and (Make("flag_skip").setValue(1))
    _rc0 = ((board[first] ].val == board[second] ].val)) and (
        if (str(_p5) == '' ):
            board
            (board) and (subprocess.call(["end_game","1"],shell=True))
            board
            Make("pieces").minus(1)
            Make("change").setValue(1)
            Make("score").plus(board[first] ].val)
            print( "joined piece from ["+str(second.val)+"] with ["+str(first.val)+"], new value="+str(board.val[str(first.val)] ])+"\n" )
        
        else:
            Make("moves").postinc()
    )

def apply_push (_p1,_p2) :
    global index_max
    global flag_skip
    global increment_max
    global i
    global j
    global k

    print( "\n\ninput: "+str(_p1)+" key\n" )
    
    i=Bash2Py(0)
    for i.val in [os.popen("_seq 0 "+str(index_max.val)).read().rstrip("\n")]:
        j=Bash2Py(0)
        for j.val in [os.popen("_seq 0 "+str(index_max.val)).read().rstrip("\n")]:
            flag_skip=Bash2Py(0)
            Make("increment_max").setValue((index_max.val - j.val))
            k=Bash2Py(0)
            for k.val in [os.popen("_seq 1 "+str(increment_max.val)).read().rstrip("\n")]:
                (flag_skip) and (break)
                push_pieces(i.val, j.val, k.val, _p1, _p2)

def check_moves () :
    Make("moves").setValue(0)
    apply_push("up", "fake")
    apply_push("down", "fake")
    apply_push("left", "fake")
    apply_push("right", "fake")

def key_react () :
    global REPLY
    global ESC

    Make("change").setValue(0)
    raw_input()
    _rc0 = ((REPLY.val  "="str(ESC.val) != '') and (
        raw_input()
        (REPLY.val  "=""[" != '') and (
            raw_input()
            
            if ( str(REPLY.val) == 'A'):
                apply_push("up")
            elif ( str(REPLY.val) == 'B'):
                apply_push("down")
            elif ( str(REPLY.val) == 'C'):
                apply_push("right")
            elif ( str(REPLY.val) == 'D'):
                apply_push("left")
        )
    )) or (
        
        if ( str(REPLY.val) == 'k'):
            apply_push("up")
        elif ( str(REPLY.val) == 'j'):
            apply_push("down")
        elif ( str(REPLY.val) == 'l'):
            apply_push("right")
        elif ( str(REPLY.val) == 'h'):
            apply_push("left")
    )

def end_game (_p1) :
    global score
    global target

    print_board()
    print( "GAME OVER\n" )
    
    print( "Your score: "+str(score.val)+"\n" )
    
    _rc0 = subprocess.call(["stty","echo"],shell=True)
    _rc0 = (_p1) and (
        print( "Congratulations you have achieved "+str(target.val)+"\n" )
        
        exit(0)
    )
    print( "You have lost, better luck next time.\033[0m\n" )
    
    exit(0)

def help (_p1) :
    subprocess.Popen("cat",shell=True,stdin=subprocess.PIPE)
    _rc0.communicate("Usage: "+str(_p1)+" [-b INTEGER] [-t INTEGER] [-l FILE] [-h]\n\n  -b			specify game board size (sizes 3-9 allowed)\n  -t			specify target score to win (needs to be power of 2)\n  -l			log debug info into specified file\n  -h			this help\n\n")
    _rc0 = _rc0.wait()

#parse commandline options
while (subprocess.call(["getopts","b:t:l:h","opt"],shell=True)):
    
    if ( str(opt.val) == 'b'):
        board_size=Bash2Py(OPTARG.val)
        (((board_size.val >= 3) & (board_size.val <= 9))) or (print( "Invalid board size, please choose size between 3 and 9\n" )
        
        exit(-1) )
    elif ( str(opt.val) == 't'):
        target=Bash2Py(OPTARG.val)
        _rcr3, _rcw3 = os.pipe()
        if os.fork():
            os.close(_rcw3)
            os.dup2(_rcr3, 0)
            _rcr4, _rcw4 = os.pipe()
            if os.fork():
                os.close(_rcw4)
                os.dup2(_rcr4, 0)
                subprocess.call(["grep","-e","^1[^1]*$"],shell=True)
            else:
                os.close(_rcr4)
                os.dup2(_rcw4, 1)
                subprocess.call(["bc"],shell=True)
                sys.exit(0)
            
        else:
            os.close(_rcr3)
            os.dup2(_rcw3, 1)
            print( "obase=2;"+str(target.val)+"\n" )
            
            sys.exit(0)
        
        (_rc0) and (print( "Invalid target, has to be power of two\n" )
        
        exit(-1) )
    elif ( str(opt.val) == 'h'):
        help(__file__)
        exit(0)
    elif ( str(opt.val) == 'l'):
        subprocess.call("exec",shell=True,std3=file('$OPTARG','wb'))
    
    elif ( str(opt.val) == '\?'):
        print( "Invalid option: -"+str(opt.val)+", try "+__file__+" -h\n" )
        
        exit(1)
    elif ( str(opt.val) == ':'):
        print( "Option -"+str(opt.val)+" requires an argument, try "+__file__+" -h\n" )
        
        exit(1)
#init board
_rc0= not Make("fields_total").setValue((board_size.val * board_size.val))
_rc0 = Bash2Py(_rc0)
_rc0= not Make("index_max").setValue((board_size.val - 1))
_rc0 = Bash2Py(_rc0)
i=Bash2Py(0)
for i.val in [os.popen("_seq 0 "+str(fields_total.val)).read().rstrip("\n")]:
    board[i]=Bash2Py(0)
_rc0= not Make("pieces").setValue(0)
_rc0 = Bash2Py(_rc0)
generate_piece()
first_round=Bash2Py(last_added.val)
generate_piece()
while (True):
    print_board()
    key_react()
    (change) and (generate_piece())
    first_round=Bash2Py(-1)
    ((pieces.val == fields_total.val)) and (check_moves()
    ((moves.val == 0)) and (end_game(0)) )
#lose the game
