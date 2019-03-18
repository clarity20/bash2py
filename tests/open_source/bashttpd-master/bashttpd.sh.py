#! /usr/bin/env python
import sys,os,subprocess,glob,re
class Bash2Py(object):
  __slots__ = ["val"]
  def __init__(self, value=''):
    self.val = value

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

#
# A simple, configurable HTTP server written in bash.
#
# See LICENSE for licensing information.
#
# Original author: Avleen Vig, 2012
# Reworked by:     Josh Cartwright, 2012
def warn () :
    print("WARNING: "+Str(Expand.at()),stderr=subprocess.STDOUT)

_rc0 = (os.access("bashttpd.conf",R_OK)) or (subprocess.Popen("cat",shell=True,stdout=file('bashttpd.conf','wb'),stdin=subprocess.PIPE)
_rc0.communicate("#\n# bashttpd.conf - configuration for bashttpd\n#\n# The behavior of bashttpd is dictated by the evaluation\n# of rules specified in this configuration file.  Each rule\n# is evaluated until one is matched.  If no rule is matched,\n# bashttpd will serve a 500 Internal Server Error.\n#\n# The format of the rules are:\n#    on_uri_match REGEX command [args]\n#    unconditionally command [args]\n#\n# on_uri_match:\n#   On an incoming request, the URI is checked against the specified\n#   (bash-supported extended) regular expression, and if encounters a match the\n#   specified command is executed with the specified arguments.\n#\n#   For additional flexibility, on_uri_match will also pass the results of the\n#   regular expression match, ${BASH_REMATCH[@]} as additional arguments to the\n#   command.\n#\n# unconditionally:\n#   Always serve via the specified command.  Useful for catchall rules.\n#\n# The following commands are available for use:\n#\n#   serve_file FILE\n#     Statically serves a single file.\n#\n#   serve_dir_with_tree DIRECTORY\n#     Statically serves the specified directory using 'tree'.  It must be\n#     installed and in the PATH.\n#\n#   serve_dir_with_ls DIRECTORY\n#     Statically serves the specified directory using 'ls -al'.\n#\n#   serve_dir  DIRECTORY\n#     Statically serves a single directory listing.  Will use 'tree' if it is\n#     installed and in the PATH, otherwise, 'ls -al'\n#\n#   serve_dir_or_file_from DIRECTORY\n#     Serves either a directory listing (using serve_dir) or a file (using\n#     serve_file).  Constructs local path by appending the specified root\n#     directory, and the URI portion of the client request.\n#\n#   serve_static_string STRING\n#     Serves the specified static string with Content-Type text/plain.\n#\n# Examples of rules:\n#\n# on_uri_match '^/issue$' serve_file \"/etc/issue\"\n#\n#   When a client's requested URI matches the string '/issue', serve them the\n#   contents of /etc/issue\n#\n# on_uri_match 'root' serve_dir /\n#\n#   When a client's requested URI has the word 'root' in it, serve up\n#   a directory listing of /\n#\n# DOCROOT=/var/www/html\n# on_uri_match '/(.*)' serve_dir_or_file_from \"$DOCROOT\"\n#   When any URI request is made, attempt to serve a directory listing\n#   or file content based on the request URI, by mapping URI's to local\n#   paths relative to the specified \"$DOCROOT\"\n#\n\nunconditionally serve_static_string 'Hello, world!  You can configure bashttpd by modifying bashttpd.conf.'\n\n# More about commands:\n#\n# It is possible to somewhat easily write your own commands.  An example\n# may help.  The following example will serve \"Hello, $x!\" whenever\n# a client sends a request with the URI /say_hello_to/$x:\n#\n# serve_hello() {\n#    add_response_header \"Content-Type\" \"text/plain\"\n#    send_response_ok_exit <<< \"Hello, $2!\"\n# }\n# on_uri_match '^/say_hello_to/(.*)$' serve_hello\n#\n# Like mentioned before, the contents of ${BASH_REMATCH[@]} are passed\n# to your command, so its possible to use regular expression groups\n# to pull out info.\n#\n# With this example, when the requested URI is /say_hello_to/Josh, serve_hello\n# is invoked with the arguments '/say_hello_to/Josh' 'Josh',\n# (${BASH_REMATCH[0]} is always the full match)\n")
_rc0 = _rc0.wait()
warn("Created bashttpd.conf using defaults.  Please review it/configure before running bashttpd again.")
exit(1) )
def recv () :
    print("< "+Str(Expand.at()),stderr=subprocess.STDOUT)

def send () :
    print("> "+Str(Expand.at()),stderr=subprocess.STDOUT)
    print( "%s\\r\\n" % (Expand.star(1)) )
    

_rc0 = (str(UID.val) == "0") and (warn("It is not recommended to run bashttpd as root."))
DATE=Bash2Py(os.popen("date +\"%a, %d %b %Y %H:%M:%S %Z\"").read().rstrip("\n"))
"RESPONSE_HEADERS=(Date: "+str(DATE.val)+" Expires: "+str(DATE.val)+" Server: Slash Bin Slash Bash)"

def add_response_header (_p1,_p2) :
    global RESPONSE_HEADERS

    RESPONSE_HEADERS+=Bash2Py("("+str(_p1)+": "+str(_p2)+")")

Str(glob.glob("HTTP_RESPONSE=([200]=OK [400]=Bad Request [403]=Forbidden [404]=Not Found [405]=Method Not Allowed [500]=Internal Server Error)"))

def send_response (_p1) :
    global HTTP_RESPONSE
    global RESPONSE_HEADERS
    global i
    global line

    code=Bash2Py(_p1)
    send("HTTP/1.0 "+str(_p1)+" "+str(HTTP_RESPONSE.val[_p1] ]))
    i=Bash2Py(0)
    for i.val in [RESPONSE_HEADERS.val[@] ]]:
        send(i.val)
    send()
    while (line = Bash2Py(raw_input())):
        send(line.val)

def send_response_ok_exit () :
    send_response(200)
    exit(0)

def fail_with (_p1) :
    send_response(_p1)
    exit(1)

def serve_file (_p1) :
    global CONTENT_TYPE
    global CONTENT_LENGTH

    file=Bash2Py(_p1)
    _rc0 = (CONTENT_TYPE = Bash2Py(raw_input())) and (add_response_header("Content-Type", CONTENT_TYPE.val))
    _rc0 = (CONTENT_LENGTH = Bash2Py(raw_input())) and (add_response_header("Content-Length", CONTENT_LENGTH.val))
    send_response_ok_exit()

def serve_dir_with_tree (_p1) :
    global tree_vers
    global tree_opts

    dir=Bash2Py(_p1)
    "tree_vers"
    "tree_opts"
    "basehref"
    "x"
    add_response_header("Content-Type", "text/html")
    # The --du option was added in 1.6.0.
    x = Bash2Py(raw_input())
    _rc0 = (str(tree_vers.val) == Str(glob.glob("v1.6*"))) and (tree_opts=Bash2Py("--du"))
    send_response_ok_exit()

def serve_dir_with_ls (_p1) :

    dir=Bash2Py(_p1)
    add_response_header("Content-Type", "text/plain")
    send_response_ok_exit()

def serve_dir (_p1) :

    dir=Bash2Py(_p1)
    # If `tree` is installed, use that for pretty output.
    _rc0 = (subprocess.call("which" + " " + "tree",shell=True,stderr=subprocess.STDOUT,stdout=file('/dev/null','wb'))
    ) and (serve_dir_with_tree(Expand.at()))
    serve_dir_with_ls(Expand.at())
    fail_with(500)

def serve_dir_or_file_from (_p1,_p2,_p3) :

    URL_PATH=Bash2Py(str(_p1)+"/"+str(_p3))
    _rc0 = subprocess.call(["shift"],shell=True)
    # sanitize URL_PATH
    URL_PATH=Bash2Py(URL_PATH.val//[^a-zA-Z0-9_os.path.expanduser("~)\-\.\/]/)
    _rc0 = (str(URL_PATH.val) == Str(glob.glob("*..*"))) and (fail_with(400))
    # Serve index file if exists in requested directory
    _rc0 = (os.path.isdir(str(URL_PATH.val)) and os.path.isfile(str(URL_PATH.val)+"/index.html") and os.access(str(URL_PATH.val)+"/index.html",R_OK)) and (URL_PATH=Bash2Py(str(URL_PATH.val)+"/index.html"))
    if (os.path.isfile(str(URL_PATH.val)) ):
        ((os.access(str(URL_PATH.val),R_OK)) and (serve_file(URL_PATH.val, Expand.at()))) or (fail_with(403))
    elif (os.path.isdir(str(URL_PATH.val)) ):
        ((os.access(str(URL_PATH.val),X_OK)) and (serve_dir(URL_PATH.val, Expand.at()))) or (fail_with(403))
    fail_with(404)

def serve_static_string () :
    add_response_header("Content-Type", "text/plain")
    send_response_ok_exit()

def on_uri_match (_p1) :
    global REQUEST_URI
    global BASH_REMATCH

    regex=Bash2Py(_p1)
    _rc0 = subprocess.call(["shift"],shell=True)
    _rc0 = (re.search(str(regex.val),str(REQUEST_URI.val))) and (subprocess.call([Str(Expand.at()),str(BASH_REMATCH.val[@] ])],shell=True))

def unconditionally () :
    global REQUEST_URI

    subprocess.call([Str(Expand.at()),str(REQUEST_URI.val)],shell=True)

# Request-Line HTTP RFC 2616 $5.1
_rc0 = (line = Bash2Py(raw_input())) or (fail_with(400))
# strip trailing CR if it exists
line=Bash2Py(line.val%%"\r")
recv(line.val)
REQUEST_METHOD = Bash2Py(raw_input())
_rc0 = (((str(REQUEST_METHOD.val) != '') and (str(REQUEST_URI.val) != '')) and (str(REQUEST_HTTP_VERSION.val) != '')) or (fail_with(400))
# Only GET is supported at this time
_rc0 = (REQUEST_METHOD.val  "=""GET" != '') or (fail_with(405))
REQUEST_HEADERS=""

while (line = Bash2Py(raw_input())):
    line=Bash2Py(line.val%%"\r")
    recv(line.val)
    # If we've reached the end of the headers, break.
    (str(line.val) == '') and (break)
    REQUEST_HEADERS+=Bash2Py("("+str(line.val)+")")
_rc0 = subprocess.call(["source","bashttpd.conf"],shell=True)
fail_with(500)
