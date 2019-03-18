#! /usr/bin/env python
import sys,os,subprocess
class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value

# Bashmarks is a simple set of bash functions that allows you to bookmark
# folders in the command-line.
#
# To install, put bashmarks.sh somewhere such as ~/bin, then source it
# in your .bashrc file (or other bash startup file):
#   source ~/bin/bashmarks.sh
#
# To bookmark a folder, simply go to that folder, then bookmark it like so:
#   bookmark foo
#
# The bookmark will be named "foo"
#
# When you want to get back to that folder use:
#   go foo
#
# To see a list of bookmarks:
#   bookmarksshow
#
# Tab completion works, to go to the shoobie bookmark:
#   go sho[tab]
#
# Your bookmarks are stored in the ~/.bookmarks file
bookmarks_file=Bash2Py(os.path.expanduser("~+"/+".+"b+"o+"o+"k+"m+"a+"r+"k+"s))
# Create bookmarks_file it if it doesn't exist
if (not os.path.isfile(str(bookmarks_file.val)) ):
    subprocess.call(["touch",str(bookmarks_file.val)],shell=True)
def bookmark (_p1) :
    global bookmark_name
    global bookmark
    global bookmarks_file
    global replace

    bookmark_name=Bash2Py(_p1)
    if (str(bookmark_name.val) == '' ):
        print("Invalid name, please provide a name for your bookmark. For example:")
        print("  bookmark foo")
    else:
        bookmark=Bash2Py(os.popen("pwd").read().rstrip("\n")+"|"+str(bookmark_name.val))
        # Store the bookmark as folder|name
        if (os.popen("grep \"|"+str(bookmark_name.val)+"\" "+str(bookmarks_file.val)).read().rstrip("\n") == '' ):
            print(bookmark.val,stdout=file('$bookmarks_file','ab'))
            print("Bookmark '"+str(bookmark_name.val)+"' saved")
        else:
            print("Bookmark '"+str(bookmark_name.val)+"' already exists. Replace it? (y or n)")
            while (replace = Bash2Py(raw_input())):
                if (str(replace.val) == "y" ):
                    # Delete existing bookmark
                    (subprocess.call("sed" + " " + "/.*|"+str(bookmark_name.val)+"/d" + " " + str(bookmarks_file.val),shell=True,stdout=file('~/.tmp','wb'))
                    ) and (subprocess.call(["mv",os.path.expanduser("~+"/+".+"t+"m+"p),str(bookmarks_file.val)],shell=True))
                    # Save new bookmark
                    print(bookmark.val,stdout=file('$bookmarks_file','ab'))
                    print("Bookmark '"+str(bookmark_name.val)+"' saved")
                    break
                elif (str(replace.val) == "n" ):
                    break
                else:
                    print("Please type 'y' or 'n'")

# Delete the named bookmark from the list
def bookmarkdelete (_p1) :
    global bookmark_name
    global bookmark
    global bookmarks_file

    bookmark_name=Bash2Py(_p1)
    if (str(bookmark_name.val) == '' ):
        print("Invalid name, please provide the name of the bookmark to delete.")
    else:
        bookmark=Bash2Py(os.popen("grep \"|"+str(bookmark_name.val)+"$\" \""+str(bookmarks_file.val)+"\"").read().rstrip("\n"))
        if (str(bookmark.val) == '' ):
            print("Invalid name, please provide a valid bookmark name.")
        else:
            (_rcr5, _rcw5 = os.pipe()
            if os.fork():
                os.close(_rcw5)
                os.dup2(_rcr5, 0)
                subprocess.call("grep" + " " + "-v" + " " + "|"+str(bookmark_name.val)+"$" + " " + str(bookmarks_file.val),shell=True,stdout=file('bookmarks_temp','wb'))
                
            else:
                os.close(_rcr5)
                os.dup2(_rcw5, 1)
                subprocess.call(["cat",str(bookmarks_file.val)],shell=True)
                sys.exit(0)
            ) and (subprocess.call(["mv","bookmarks_temp",str(bookmarks_file.val)],shell=True))
            print("Bookmark '"+str(bookmark_name.val)+"' deleted")

# Show a list of the bookmarks
def bookmarksshow () :
    global bookmarks_file

    _rcr1, _rcw1 = os.pipe()
    if os.fork():
        os.close(_rcw1)
        os.dup2(_rcr1, 0)
        subprocess.call(["awk","{ printf \"%-40s%-40s%s\\n\",$1,$2,$3}","FS=\|"],shell=True)
    else:
        os.close(_rcr1)
        os.dup2(_rcw1, 1)
        subprocess.call(["cat",str(bookmarks_file.val)],shell=True)
        sys.exit(0)
    

def go (_p1) :
    global bookmark_name
    global bookmark
    global bookmarks_file
    global dir

    bookmark_name=Bash2Py(_p1)
    bookmark=Bash2Py(os.popen("grep \"|"+str(bookmark_name.val)+"$\" \""+str(bookmarks_file.val)+"\"").read().rstrip("\n"))
    if (str(bookmark.val) == '' ):
        print("Invalid name, please provide a valid bookmark name. For example:")
        print("  go foo")
        print()
        print("To bookmark a folder, go to the folder then do this (naming the bookmark foo):")
        print("  bookmark foo")
    else:
        dir=Bash2Py(os.popen("echo \""+str(bookmark.val)+"\" | cut -d| -f1").read().rstrip("\n"))
        os.chdir(str(dir.val))

def _go_complete (_p1,_p2) :
    global bookmarks_file

    # Get a list of bookmark names, then grep for what was entered to narrow the list
    _rcr1, _rcw1 = os.pipe()
    if os.fork():
        os.close(_rcw1)
        os.dup2(_rcr1, 0)
        _rcr2, _rcw2 = os.pipe()
        if os.fork():
            os.close(_rcw2)
            os.dup2(_rcr2, 0)
            subprocess.call(["grep",str(_p2)+".*"],shell=True)
        else:
            os.close(_rcr2)
            os.dup2(_rcw2, 1)
            subprocess.call(["cut","-d\|","-f2"],shell=True)
            sys.exit(0)
        
    else:
        os.close(_rcr1)
        os.dup2(_rcw1, 1)
        subprocess.call(["cat",str(bookmarks_file.val)],shell=True)
        sys.exit(0)
    

_rc0 = subprocess.call(["complete","-C","_go_complete","-o","default","go"],shell=True)
