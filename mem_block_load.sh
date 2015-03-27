#!/bin/bash

sudo insmod mem_block.ko mem_block_major=299 delay=3
sudo mknod /dev/mem_block c 299 0
sudo chmod 666 /dev/mem_block
cat /dev/mem_block
