#!/bin/bash
count=0
while true
do
	let count++
	echo test $count:
	./tcp_client
	sleep 1
done
