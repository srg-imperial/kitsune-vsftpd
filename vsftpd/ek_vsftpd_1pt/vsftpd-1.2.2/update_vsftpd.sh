#!/bin/bash
# Execution: run from the directory that contains the version being upgraded to.
# Updates all "driver" processes

if [ ! -e vsftpd.so ]
then 
	echo "vsftpd.so does not exist!"
	exit
fi

pids=$(pidof driver)
for i in $pids
do
	sudo ../../../../src/doupd $i `pwd`/vsftpd.so
	echo "Updated $i"
done
