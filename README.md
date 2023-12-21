# bash2py
Bash-to-python transpiler<br>
Latest version: 3.6 (with 4.0 anticipated)

Welcome to the new home of the Bash2Py project. The project's original home is  https://www.swag.uwaterloo.ca/bash2py/index.html<br>

Bash2py is a tool designed to automate the translation of code from Bash to Python.

We have attempted to ensure that the source code for Bash 4.3.30 remains intact within this distribution. All changes to this source code (by intent) are to be compiled if and only if the BASH2PY macro is defined. This allows ready discovery of how the original source code has been changed.

The **main** files changed are:
1.  Added:  fix_string.c<br>
    This attempts to mimic the bash command line transforms performed on the input prior to execution.

2.  Added:  translate.c<br>
    This attempts to translate high level bash commands. It is derived from print_cmd.c

3.  Added: translate_expr.c<br>
    This parses arithmetic expressions.  While the original code in expr.c did this to compute an arithmetic result, the modified code does this to reformat bash arithmetic expressions into (it is hoped) appropriate equivalent Python expressions.

4.  Changed & added: burp.c, dynamo.c<br>
    These files contain internals of the buffering, logging and code generation APIs for the transpiler.

The document `Bash2pyManual.html` is a heavily modified version of the Bash reference manual that summarizes the progress of this project. It indicates which Bash constructs are likely to be translated correctly and which have not been addressed. Items highlighted in green are likely to be transpiled correctly, those in orange may translate correctly some of the time, and those in red are not expected to be translated correctly. Text in purple provides additional commentary.

Considerable work remains to be done on this tool.  We distribute it as-is, with no warranty or guarantee as to its functionality whatsoever.

### Contact:

Please reach out to the project maintainers via this online repository.
The historical project coordinator, Ian Davis at textserver.com@gmail.com, is no longer maintaining the project.

WARNING: It is highly recommended that you backup any file/directory you wish to translate before running bash2py on that file/directory. This is prototype software, and we do not guarantee the outcome.

### Instructions for installation:

- Working in Windows? We recommend the `Cygwin` Unix emulation layer and the `bash-devel` and `cygwin-devel` packages to provide all required header files. The standard GNU toolchain (gcc, make, autoconf) will enable you to build the project.

- Working on Android? The standard GNU toolchain is adequate to build the project **except** for at least one symbol (mblen) that is missing from the libc system library. You can download this code from https://github.com/termux/libandroid-support and build and link it yourself to create the executable.

- Download the latest version of this software

- `cd` into the bash2py project

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

If the -h/--html option is specified, a comparative before and after translation html file is generated rather than a python file. This is useful for observing the details of the translation process.

`<SCRIPT>` is replaced by the name of the script you want to translate, and `<DIRECTORY>` is the name of the directory you want to recursively translate.

Bash2py will put the translated Python file in the same directory as the corresponding Bash file, with the same name, but with a .py extension. HTML files will be assigned a .html extension.

Note: Bash2py is not perfect. You will have to review and edit the translated file by hand, to fix any errors that bash2py made in translation.

