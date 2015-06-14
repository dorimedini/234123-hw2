#!/bin/bash


i=1
mkdir /mnt/hgfs/HW/234123-hw2/scripts/Test_Objects

for test in `ls ;
	do
		echo $test
		gcc $test -o /mnt/hgfs/HW/234123-hw2/scripts/Test_Objects/test${i}
		let i++
		
	done


i=1
for test in `ls /mnt/hgfs/HW/234123-hw2/scripts/Test_Objects/`;
	do
		echo --------------------------
		echo Running test $i
		/mnt/hgfs/HW/234123-hw2/scripts/Test_Objects/./$test
		let i++
		
	done

rm /mnt/hgfs/HW/234123-hw2/scripts/Test_Objects/*
rmdir /mnt/hgfs/HW/234123-hw2/scripts/Test_Objects