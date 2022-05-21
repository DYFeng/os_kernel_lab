#!/bin/sh
set -x
pkill -f -9 qemu-system-i386

cd /home/gauss/Projects/os_kernel_lab/labcodes/lab2
make clean
make -j10
#bochs -f /home/gauss/Projects/os_kernel_lab/bochsrc -q &
qemu-system-i386 -S -s -hda /home/gauss/Projects/os_kernel_lab/labcodes/lab2/bin/ucore.img &
