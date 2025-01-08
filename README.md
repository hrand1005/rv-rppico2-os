# Title TBD

RISC-V RTOS for rp2350 and Raspberry Pi Pico 2, "bare-metal" implementation. 

## Dependencies

- Raspberry Pi Pico 2
- Raspberry Pi Debug Probe (or equivalent)
- RISC-V Toolchain (GCC, GDB, etc)
- OpenOCD

See bootstrap repo for more info on this: https://github.com/hrand1005/raspberry-pi-pico-2

## Setup

After installing the necessary dependencies, setting up udev rules for the
pico and debug probe, etc, you can choose a "user application" to run on the
kernel. 

In one terminal window, establish a connection with the debug probe and
configure it for debugging the pico 2:

```
make console
```

Then, in a separate window, compile and run the user application (e.g. for
blinky):

```
make run APP=blinky
```

## Project Layout

- `kernel`  - 
- `user`    - user applications and libraries
- `include` - definitions common to both kernel and user code 
