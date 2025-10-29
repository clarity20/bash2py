#!/usr/bin/env bash
sed -i -r '635 {s/_to_full_posix//;s/\)/, PATH_MAX)/}' general.c
sed -i -r '295 {s/__private_//;s/__//;s/ =/; \/\//}' lib/termcap/termcap.c
sed -i -r '126,127 {s/__private_//;s/__//;}' lib/termcap/tparam.c

