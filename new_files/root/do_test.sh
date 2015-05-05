#!/bin/bash

/mnt/hgfs/HW/234123-hw2/scripts/update_files.sh
gcc sched_tester.c -o sched_tester
./sched_tester 10 35 10 47 10 5 10 20
dmesg > log

