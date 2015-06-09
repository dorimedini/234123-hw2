#!/bin/bash

/mnt/hgfs/HW/234123-hw2/scripts/update_files.sh

# Our tester:
gcc yet_another_tester.c -o yet_another_tester
./yet_another_tester
dmesg > yet_another_log

