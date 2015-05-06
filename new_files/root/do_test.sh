#!/bin/bash

/mnt/hgfs/HW/234123-hw2/scripts/update_files.sh

# Our tester:
gcc sched_tester.c -o sched_tester
./sched_tester 1 10 50 5 40 43 50 10 > hw2_log
./sched_tester 3 45 1 5 50 7 >> hw2_log
./sched_tester 44 10 43 20 2 25 3 30 50 35 4 40 5 45 >> hw2_log
dmesg > log

