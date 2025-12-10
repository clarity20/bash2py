#!/usr/bin/env bash

# Try out some string substitutions with literal, variable, and globbing forms.
s=01234567890abcdefgh
key=7
repl=haha
echo ${s/+(90|qr)/__}
echo ${s/456/___}
echo ${s//456/___}
echo ${s/$key/_${repl}_}
echo ${s/8*a??/__}

