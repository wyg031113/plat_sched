#!/bin/bash
while [ 1 -eq 1 ];
do
pida=` ps aux| grep testA|grep -v 'grep'|awk '{print $2}'`
acpu=`ps aux| grep testA|grep -v 'grep'|awk '{print $3}' `
amem=`ps aux| grep testA|grep -v 'grep'|awk '{print $4}' `

pidb=` ps aux| grep testB|grep -v 'grep'|awk '{print $2}'`
bcpu=`ps aux| grep testB|grep -v 'grep'|awk '{print $3}' `
bmem=`ps aux| grep testB|grep -v 'grep'|awk '{print $4}' `

echo "-------BEGIN--------"
echo "testA's resources:"
echo "file descripter:"
ls /proc/$pida/fd/
echo CPU: $acpu%
echo mem: $amem%
echo ""
echo "testB resources:"
echo "file descripter:"
ls /proc/$pidb/fd/
echo CPU: $bcpu%
echo mem: $bmem%
echo "-------END--------"

sleep 2
echo ""
done
