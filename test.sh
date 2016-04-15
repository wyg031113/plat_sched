#!/bin/bash
count=0
t=0
while true
do
	let count++
	echo test $count:
	./testB &
	echo "hehehehe"
	t=0
	while true
	do
		let t++
		echo "time=$t"
		if [ $t -ge 3 ]; then
			pkill testB
			echo "stopped testB"
			sleep 3
			break
		fi
		sleep 1
	done
done
