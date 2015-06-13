#!/bin/bash

# Go to the dir
cd /root/
mkdir -p hw2
cd /root/hw2/
mkdir -p tests

# Recursively dos2unix-ify all edited files before copying, so
# no need to recurse over all files in /usr/src/linux-2.4.18-14custom/
dos2unix /mnt/hgfs/HW/234123-hw2/submission_appeal/kernel/*
dos2unix /mnt/hgfs/HW/234123-hw2/new_files/*

# Copy all files. Assume the correct paths in the edited/ directory.
cp -vr /mnt/hgfs/HW/234123-hw2/submission_appeal/kernel/* /usr/src/linux-2.4.18-14custom/
cp -vr /mnt/hgfs/HW/234123-hw2/new_files/* ./
cp -vr /mnt/hgfs/HW/234123-hw2/appeal_test/* ./tests/
