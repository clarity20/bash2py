
R=$(cat /etc/redhat-release)


secretNumber=$(( ((`date +%N` / 1000) % 100) +1 ))
guess=-1
while [ "$guess" != "$secretNumber" ]; do
	echo -n "I am thinking of a number between 1 and 100. Enter your guess:"
	read guess
	if [ "$guess" = "" ]; then
		echo "Please enter a number."
	elif [ "$guess" = "$secretNumber" ]; then
		echo -e "\aYes! $guess is the correct answer!"
	elif [ "$secretNumber" -gt "$guess" ]; then
		echo "The secret number is larger than your guess. Try again."
	else
		echo "The secret number is smaller than your guess. Try again."
	fi
done
