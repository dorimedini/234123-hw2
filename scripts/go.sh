#!/bin/bash

# Update all files
./update_files.sh

# Now, build the kernel
cd /usr/src/linux-2.4.18-14custom
make bzImage
cd arch/i386/boot
cp -f bzImage /boot/vmlinuz-2.4.18-14custom