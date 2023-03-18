# bash2py
Bash-to-python transpiler    https://www.swag.uwaterloo.ca/bash2py/index.html<br>
Current version: 3.0

Bash2py is a tool designed to automate the translation from Bash to Python.

Coding has attempted to ensure that the source code for Bash 4.3.30 remains<br>
entact within this distribution. All changes to this source code (by intent)<br>
are to be compiled if and only if the BASH2PY macro is defined.  This<br>
allows ready discovery of how the original source code has been changed.<br>

The three main files changed are:
1.  Added:  fix_string.c<br>
    This attempts to mimic the bash command line transforms performed on the<br>
    input prior to execution.

2.  Added:  translate.c<br>
    This attempts to translate high level bash commands. It is derived<br>
    from print_cmd.c

3.  Changed: expr.c<br>
    This parses arithmetic expressions.  While the original code did this<br>
    to compute an arithmetic result, the modified code does this to reformat<br>
    bash arithmetic expressions into (it is hoped) appropriate equivalent<br>
    Python expressions.

Documentation attempting to describe those Bash constructs likely to be<br>
translated correctly by this tool, can be found at:

http://www.swag.uwaterloo.ca/bash2py/Bash2pyManual.html

This document is a heavily modified version of the Bash reference manual,<br>
permitting easy comprehension of all the features of Bash and which of<br>
these Bash2Py is likely to be able to handle.

Items highlighted in green have a good chance of translating correctly, those<br>
in orange may translate correctly some of the time, and those in red are not<br>
expected to be translated anywhere near correctly.  Text in purple provides<br>
additional commentary.

Considerable work on this tool remains to be done.  It is stressed that it<br>
is distributed as is, without any warrantee or assurance as to its<br>
functionality whatsoever.

Currently contact:

Ian Davis at textserver.com@gmail.com to report concerns, or to request<br>
work be undertaken to further improve this product. 

WARNING: It is highly recommended that you backup any file/directory you<br>
wish to translate, before running bash2py on that file/directory.<br>
This is essentially protype software and mileage will vary.

### Instructions for installation:

- Download the latest version of this software from swag.uwaterloo.ca/bash2py

- `cd` into the project

- To setup, run this command:

```console
./install
```

### Usage

Run bash2py in any of the following ways:

1. Using the front end interface to invoke bash2pyengine

```console
./bash2py [-h] -f <SCRIPT>
./bash2py [-h] -d <DIRECTORY>
./bash2py [-h] <SCRIPT|DIRECTORY>
```

2. Invoking this engine directly

```console
./bash/bash2pyengine [--html] <SCRIPT>
```

If the -h/--html option is specified a comparative before and after translation<br>
html file is generated rather than a python file. This is useful for observing<br>
the details of the translation process.

`<SCRIPT>` is replaced by the name of the script you want to translate, and<br>
`<DIRECTORY>` is the name of the directory you want to recursively translate.

Bash2py will put the translated Python file in the same directory as the<br>
corresponding Bash file, with the same name, but with a .py extension. HTML<br>
files will be assigned a .html extension.

Note: Bash2py is not perfect. You will have to review and edit the translated<br>
file by hand, to fix any errors that bash2py made in translation.
