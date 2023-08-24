#!/bin/bash
FILE=$1

if [ -f $FILE ];
then
   echo "File $FILE exists."
else
   echo "File $FILE does not exist."
fi

if [ -b $FILE ];
then
    echo "File $FILE exists and is a block-special file."
else
    echo "Not a block-special file"
fi
