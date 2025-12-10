#!/usr/bin/env bash

# Try out some string chops with literal, variable, and globbing forms.
s=aasdf90babasfgbbacxv
key=f
repl=acxv
echo ${s#a*f}
echo ${s##a*f}
echo ${s%a*v}
echo ${s%%a+(?)*v}   # Known bug; requires glob-to-regex to run recursively
echo ${s#+(a)*$key}
echo ${s##+(a)*${key}__}
echo ${s%%+(b)$repl}

