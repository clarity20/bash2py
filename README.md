# bash2py: A Bash-to-python transpiler<br>
Latest version: 3.7 (with 4.0 anticipated)

Welcome to the new home of the Bash2Py project. *The original home is [here](https://www.swag.uwaterloo.ca/bash2py/index.html)*<br>

*As of September 2025, bash2py version 3.7 is available under the project tag "v3.7"
while the live branch is rearchitected in anticipation of version 4.0.*<br>

Bash2py translates Bash shell code into Python.

We have attempted to ensure that the source code for Bash 4.3.30 remains intact within this distribution. All changes to this source code (by intent) are to be compiled if and only if the BASH2PY macro is defined. This enables ready discovery of changes to the original source code.

The **main** files changed are:
1.  Added:  fix_string.c<br>
    *Mimics shell expansion and substitution so bash expressions can be rendered in python*

2.  Added:  translate.c<br>
    *Translates bash code at the level of command syntax. Derived from bash's print_cmd.c.*

3.  Added: translate_expr.c<br>
    *Translates arithmetic expressions. This is an adaptation of bash's expr.c. Instead of performing computations, we reformat bash arithmetic expressions into equivalent Python expressions.*

4.  Changed & added: burp.c, dynamo.c<br>
    *These files implement the buffering, logging and code generation APIs for the transpiler.*

The document `Bash2pyManual.html` is a modified version of the Bash reference manual that summarizes the progress of this project. It indicates which Bash constructs we have specifically addressed. Items highlighted in green are likely to be transpiled correctly, those in orange may translate correctly some of the time, and those in red are not expected to work *yet*. Text in purple provides additional commentary.

This tool is a work in progress. We offer no warranty or guarantees as to its functionality. is a violation of federal law to use this software in a manner inconsistent with its labeling. Try it, but do not inhale it.

### Contact:

Please reach out to the project maintainers via this online repository.
The historical project coordinator, Ian Davis at textserver.com@gmail.com, is no longer maintaining the project.

WARNING: It is highly recommended that you backup any file/directory you wish to translate before running bash2py on that file/directory.

### Instructions for installation:
- You can build and run bash2py on Linux, Windows, Mac and Android.

- Working in Windows? We recommend the `Cygwin` Unix emulation layer and the `bash-devel` and `cygwin-devel` packages to provide all required header files. The standard GNU toolchain (gcc, make, autoconf) will enable you to build the project.

- Working on Android? The standard GNU toolchain is adequate to build the project **except** for at least one symbol (mblen) that is missing from the libc system library. You can download this code from https://github.com/termux/libandroid-support and build and link it to create the executable.

- Download version 3.7 of this software

- `cd` into the bash2py project and run:

```console
./install
```
This will compile the `bash2pyengine` binary which you can run directly or through `./bash2py`; see "Usage".

### Usage
*The usage is slightly nonstandard and is being modernized for version 4.0. Please stay tuned.*

Run bash2py in any of the following ways:

1. Using the front end interface to invoke bash2pyengine

```console
./bash2py [-h] -f <SCRIPT>
./bash2py [-h] -d <DIRECTORY>
./bash2py [-h] <SCRIPT|DIRECTORY>
```

2. Invoking this engine directly

```console
./bash/bash2pyengine [--html] <SCRIPT> [...]
```

Bash2py will write the translated Python file(s) to the same directory as the corresponding Bash input file(s) using the same basename but with a *.py* extension.

If the -h/--html option is specified, an html file with an *.html* extension will be generated to assist in visually following the translation process. 

