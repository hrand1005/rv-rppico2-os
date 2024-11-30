#!/bin/env bash
#
# To be run after putting the rp2350 in BOOTSEL mode.

mount /dev/sda1 /mnt/pico
cp /home/rand/workspace/pico/pico-examples/build/blink/blink.uf2 /mnt/pico
# cp /home/rand/workspace/pico/pico-examples/build/blink2/blink2.uf2 /mnt/pico
umount /mnt/pico
