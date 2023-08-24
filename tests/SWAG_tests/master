export x=2asdfasdbxcb
export y=3asdfasdzsdgzs

# Calculate the average of a series of numbers.

SCORE="0"
AVERAGE="0"
SUM="0" # comment at end of line
# comment at beginning of line
NUM="0"  

foo=`cat /tmp/baz`

while true; do

  echo -n "Enter your score [0-100%] ('q' for quit): "; read SCORE;

  if (("$SCORE" < "0"))  || (("$SCORE" > "100")); then
    echo "Be serious.  Common, try again: "
  elif [ "$SCORE" == "q" ]; then
    echo "Average rating: $AVERAGE%."
    break
  else
    SUM=$[$SUM + $SCORE]
    NUM=$[$NUM + 1]
    AVERAGE=$[$SUM / $NUM]
  fi

done

echo "Exiting."


# Cleanup
# Run as root, of course.

cd /var/log
cat /dev/null > messages
cat /dev/null > wtmp
echo "Log files cleaned up."

Msg=`printf "%s %s \n" $Message1 $Message2`
echo $Msg; echo $Msg
