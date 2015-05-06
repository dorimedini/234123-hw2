#!/bin/bash

/mnt/hgfs/HW/234123-hw2/scripts/update_files.sh

# Facebook tester:
gcc code_test.c -o code_test
./code_test
dmesg > log

