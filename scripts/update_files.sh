#!/bin/bash

# Recursively dos2unix-ify all edited files before copying, so
# no need to recurse over all files in /usr/src/linux-2.4.18-14custom/
dos2unix /mnt/hgfs/HW/234123-hw2/kernel_files/edited/*
dos2unix /mnt/hgfs/HW/234123-hw2/new_files/*

# Copy all files. Assume the correct paths in the edited/ directory.
cp -vr /mnt/hgfs/HW/234123-hw2/kernel_files/edited/* /usr/src/linux-2.4.18-14custom/
cp -vr /mnt/hgfs/HW/234123-hw2/new_files/* /