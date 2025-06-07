#!/usr/bin/env bash
if (($# < 1)); then
    echo Usage: "$0" word
    exit 0
fi

if [[ "$1" =~ r(.) ]]; then
    echo "$1" contains r plus ${BASH_REMATCH[1]}.
    br0=${BASH_REMATCH[0]}
fi

if [[ "$1" =~ x ]]; then
    br0=${BASH_REMATCH[0]}
    echo "$1" contains x.
elif [[ "$1" =~ w ]]; then
    br0=${BASH_REMATCH[0]}
    echo "$1" contains w.
fi

echo To reiterate '(x>w>r.)', "$1" contains $br0.

[[ "$1" =~ r(.) ]]
echo Successor of \'r\' in "$1" is ${BASH_REMATCH[1]}.

