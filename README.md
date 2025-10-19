# bash2py: A Bash-to-python transpiler

Welcome to the new home of `bash2py`, a project for translating Bash shell code into Python.

For an overview of the project's status (i.e. its current capabilities), the document `Bash2pyManual.html` is a modified version of the Bash reference manual that summarizes the progress of this project. It indicates which Bash constructs we have specifically addressed. Items highlighted in green are likely to be transpiled correctly, those in orange may translate correctly some of the time, and those in red are not expected to work *yet*. Text in purple provides additional commentary.

### Installation and usage

You can install and run `bash2py` on Linux, Windows, Mac and Android.

**Steps:**

1. Download this software package.

2. Set up the standard GNU toolchain for building C-language projects (gcc, make, autoconf).

*Building on Windows?* We recommend the `Cygwin` Unix emulation layer and the `bash-devel` and `cygwin-devel` packages to provide all required header files.

*Building on Android?* The standard GNU toolchain is adequate to build the project **except** for at least one symbol (mblen) that is missing from the libc system library. You can download this code from https://github.com/termux/libandroid-support and build and link it to create the executable.

3. `cd` into the bash2py project and run

```
./install.sh
```

This will build `bash2py [.exe]`.

4. **Run bash2py:**

```
./bash2py  shellScript  [...]
```

`Bash2py` will write its Python translation(s) to the same directory as the corresponding Bash file(s) using the same basename but a *.py* extension.
It will also write semi-customizable transpilation logs to your screen.

`Bash2py` is a work in progress. We offer no warranty or guarantees as to its output. Please backup any input files before running.

It is a violation of federal law to use this software in a manner inconsistent with its labeling. Try it, but do not inhale.

### Development notes:

Our methodology is to alter and extend the native Bash source code, using the macro BASH2PY to segregate our changes.

The most significant changes and additions are:
1.  (Added)  fix_string.c
    *Mimics shell expansion and substitution so bash value-expressions can be rendered in python*

2.  (Added)  translate.c
    *Translates bash command syntax into python equivalents; "echo hello" becomes "print('hello')". Derived from bash's print_cmd.c.*

3.  (Added)  translate_expr.c
    *Translates arithmetic expressions. This is an adaptation of bash's expr.c. Instead of performing computations, we reformat bash arithmetic expressions into equivalent Python expressions.*

4.  (Changed & added)  burp.c, dynamo.c
    *These files implement the buffering, logging and code generation subsystems for the transpiler.*

The bash source code comes from bash-4.3.30 but a forward port is on the agenda.

### Contact:

Please reach out to the project maintainers via this online repository.
The historical project coordinator, Ian Davis at textserver.com@gmail.com, is no longer maintaining the project.

*Visit the original project home [here](https://www.swag.uwaterloo.ca/bash2py/index.html)*.

