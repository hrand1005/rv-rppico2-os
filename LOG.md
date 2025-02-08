# RISC-V RTOS for RP2350 Log

Logging the development of an operating system for the raspberry pi pico 2.
The OS will be compiled for the RISC-V architecture (because I want to) and will
thus run on the hazard3 cores in the rp2350 processor.

I'm writing this RTOS because I'm interested in learning more about the
following:

- Bare-metal Programming
- Operating Systems Design and Implementation
- RISC-V Architecture & RISC-V ASM programming
- Multicore Scheduling 
- Interrupt Handling
- Embedded Systems Security
- Systems Performance Profiling and Optimization
- Managing Projects with GNUMake
- Testing Embedded Systems

## Getting Started (? - 11/26/24)

I soldered header pins onto the board, got myself a debug probe, and poked around
the documentation and tooling for the rp2350 and pico 2. After getting familiar
with the tooling, I developed some simple utilities for bootstrapping the system
without the pico-sdk. You can find them at github.com/hrand1005/raspberry-pi-pico-2.
They include a `Makefile`, assembly (`startup.S`), a linker script (`memmap.ld`), and
`init.gdb`. To run them, you'll need `openocd`, the riscv gnu toolchain, among other
things. I'd recommend going through the raspberry pi pico 2 "getting started"
document to set up the proper tooling.

At the bottom of `startup.S` is an "image definition". This is a special block of
bytes that indicates to the immutable bootrom that the binary we're flashing onto
the board is a risc-v executable that we want to be called. The image definition is
currently identical to the minimal image provided in the rp2350 datasheet section
5.9.5.2 "Minimum RISC-V IMAGE_DEF" [3].

## Bootloader (11/27/24 - ?)

### Vector Table (11/27/24)

Unlike ARM, RISC-V requires you to define how your vector table will operate.
You can set the two LSBs in the `mtvec` register to indicate which MODE the 
vector table will operate in, 0 for Direct or 1 for Vectored.

From the privileged isa spec [1]:

_"When MODE=Direct, all traps into machine mode cause the pc to be set to the
address in the BASE field. When MODE=Vectored, all synchronous exceptions into
machine mode cause the pc to be set to the address in the BASE field, whereas
interrupts cause the pc to be set to the address in the BASE field plus four
times the interrupt cause number."_

In other words:
```
if trap is exception:
    jump to BASE
else:
    jump to BASE + 0x4 * cause
```

Thus, we must handle exceptions and interrupts (both types of "traps" as defined
by the RISC-V privileged spec [1]) separately. In addition to vectoring, the
control status register `mcause` can be used to understand the cause of the 
interrupt or exception. This also corresponds to bits set in the `mip` or `mie`
registers (`mip` = interrupts pending, `mie` = interrupts enabled).

An interrupt is signaled as pending by setting a bit in the `mip` register.
Each bit position in the register may represent a different interrupt cause.
This also corresponds to the control status register `mcause`, written when
a trap is taken into machine mode with an exception code.

The rp2350 datasheet section 3.8.4.2 [3] specifies that the following interrupts
are possible:

- `meip: cause 11` --> Machine External Interrupt
- `mtip: cause 7`  --> Machine Timer Interrupt
- `msip: cause 3`  --> Machine Software Interrupt

> NOTE: when multiple interrupts are active, the hardware handles them in `meip` -> 
`msip` -> `mtip` order, NOT an order related to the cause number.
_"Multiple simultaneous interrupts destined for M-mode are handled in the
following decreasing priority order: MEI, MSI, MTI, SEI, SSI, STI." [1]

Note also that while these "registers" are read only, but memory-mapped reigsters
(for us defined in [3]) allow us to write to them.

Let's then define a vector table for handling traps.
There are four cases:

1. An exception is triggered, jump to BASE.
2. An interrupt is triggered with cause 3, jump to BASE + 12
3. An interrupt is triggered with cause 7, jump to BASE + 28
4. An intterupt is triggered with cause 11, jump to BASE + 44

Thus we can mount the vector table at the address for the instruction to
handle (1), and ensure instructions to handle 2-4 are at an appropriate offest.

```
__vector_table:
j isr_exc      /* BASE +  0 */ 
.word 0        /* BASE +  4 */ 
.word 0        /* BASE +  8 */ 
j isr_msi      /* BASE + 12 */ 
.word 0        /* BASE + 16 */ 
.word 0        /* BASE + 20 */ 
.word 0        /* BASE + 24 */ 
j isr_mti      /* BASE + 28 */ 
.word 0        /* BASE + 32 */ 
.word 0        /* BASE + 36 */ 
.word 0        /* BASE + 40 */ 
j isr_mei      /* BASE + 44 */ 
```

The rp2350 datasheet indicates that when vectoring is enabled, mtvec must be
aligned to the size of the table rounded up to the nearest power of two, in
this case, 64 bytes. We can use `.p2align 6` to align to 64 bytes:

```
.p2align 6
__vector_table:
j isr_exc      /* BASE +  0 */ 
.word 0        /* BASE +  4 */ 
.word 0        /* BASE +  8 */ 
j isr_msi      /* BASE + 12 */ 
.word 0        /* BASE + 16 */ 
.word 0        /* BASE + 20 */ 
.word 0        /* BASE + 24 */ 
j isr_mti      /* BASE + 28 */ 
.word 0        /* BASE + 32 */ 
.word 0        /* BASE + 36 */ 
.word 0        /* BASE + 40 */ 
j isr_mei      /* BASE + 44 */ 
```

We must also mount the vector table using `csr` instruction and `mtvec`. We
set the LSB to 1 to ensure vectoring is enabled.

```
la a0, __vector_table
ori a0, a0, 1
csrw mtvec, a0
```

Finally, we should create weak definitions for the points our vector table
jumps to so that we know when the corresponding trap occurred: 

```
.weak isr_exc
isr_exc:
    ebreak
    j jail

.weak isr_msi
isr_msi:
    ebreak
    j jail

.weak isr_mti
isr_mti:
    ebreak
    j jail

.weak isr_mei
isr_mei:
    ebreak
    j jail
```

### Testing Vector Table (11/28/24)

Defining the interrupt table is only half the battle; enabling and disabling
traps, as well as enabling and disabling specific types of interrupts, requires
understanding both a variety of control status registers (CSRs) in RISC-V as
well as the provided rp2350 memory mapped registers that implement interrupt
enabling/disabling and pending/clearing interrupt requests.

The slough of acronyms in the privileged spec and rp2350 datasheet are a bit
confusing, so let me clarify a few terms:

mstatus = machine status register
mie  =    machine interrupt enable
mei  =    machine external interrupt
meie =    machine external interrupt enable

It is important not to confuse these terms, and also to undertsand that
some of these acronyms are used both as the names of positions within
a register (e.g. `mstatus.mie`) AND as registers themselves. For example,
`mstatus.mie` indicates position 3 in the `mstatus` register, but `mie` (by 
itself) is also a register that can be configured.
 
`mstatus` may be used for multiple purposes to modify the machine state,
it is a "higher level" register that is used for multiple purposes. 
`mstatus.mie` represents the machine interrupt enable (globally).
By setting `mstatus.mie` to 1, for example, we have generally made
interrupts possible, but we have not specified what kind of interrupts
are enabled.

Separately, there is the `mie` register. `mie.meie` represents the bit position in
`mie` that can enable external interrupts. Its position corresponds to the cause
(11), and by setting this bit we enable machine external interrupts.

So we will need to enable machine interrupts (`mstatus.mie`), and specifically
machine external interrupt requests `mie.meie`. But machine external interrupt
requests _themselves_ may occur for a number of reasons, and each one is manifested
by pending a interrupt request (IRQ). It's at this level of granularity that the
riscv privileged spec starts delegating interrupt enabling/disabling/pending to 
the processor and interrupt controller implementation.

First, though, you should know that the riscv privileged spec defines the machine
interrupt pending register `mip`, which indicates which kinds of interrupts are
pending (duh). There is a bit position `mip.meip` that indicates a machine
external interrupt is pending (again, position 11). Importantly, regarding this
register, the spec says this:

_"Each individual bit in register `mip` may be writable or may be read-only. When
bit i in `mip` is writable, a pending interrupt i can be cleared by writing 0 to
this bit. If interrupt i can become pending but bit i in mip is read-only, the
implementation must provide some other mechanism for clearing the pending
interrupt."_ [1]

And given that we are currently looking at machine external interrupts, we can 
further see what the spec says about `mip.meip`:

_"Bits mip.MEIP and mie.MEIE are the interrupt-pending and interrupt-enable bits
for machine-level external interrupts. MEIP is read-only in mip, and is set and
cleared by a platform-specific interrupt controller."_ [1]

Thus, we must turn to the rp2350 datasheet to understand the machine external
interrupt control mechanisms. We see in section 3.8.6.1 that there are additional
CSRs defined to control up to 512 interrupts via bit vectors [3]. Each of the 512
external interrupts can be separately enabled, disabled, pended, and even
assigned priorities using these CSRs. Notably, the implementation provides
`MEIEA` and `MEIFA`.

`MEIEA` can be used to enable machine external interrupts based on their IRQ.
`MEIFA` can be used to force ("force") machine external interrupts based on their IRQ.
Both registers act as a sliding window, where the 4 LSBs indicate an index into a 
16-bit window o fthe 512 bit array. 

On startup, it's good practice to clear the enabled and pending IRQs. According to
the rp2350 datasheet, the reset clears all of the enabled bits. We can manually clear
the pending bits in a loop over the relevant bits accessible by indexes into the 
512 bit pending vector via MEIFA.

With this knowledge, we can add to our bootloader program to do some minimal
initialization of interrupt handling in our operating system.

```
    /*
     * clear all IRQ force array bits (MEIFA at offset 0xbe2)
     * We loop 32 times to clear 16 * 32 = 512 force bits.
     */
    li a0, 32
clear_meip:
    csrw 0xbe2u, a0
    addi a0, a0, -1
    bgtz a0, clear_meip
    
    /*
     * enable machine external interrupts (MEIE at offset 0x800)
     */
    li a0, 0x800u
    csrw mie, a0
```

Now we can set the global interrupt enable (`mstatus.mie`):
```
    csrsi mstatus, 0x8u
```

With this done, we should perform a quick test to check that machine external
interrupts are properly enabled. Let's enable machine external interrupt
request number 0 in MEIEA:

```
    li a0, 0x10000u
    csrw 0xbe0u, a0
```

...then trigger the interrupt by setting the interrupt pending bit in MEIFA:

```
    csrw 0xbe2u, a0
    fence.i
```

Looks good! We'll revisit interrupt handling later on as needed and when we
studying the hazard3 interrupt controller.

### XIP Subsystem (11/28/24 - 11/30/24)

The XIP subsystem refers to a 16KB cache that speeds up accesses to flash
memory. To set up the XIP subsystem, we must understand a little about how
the processor bootrom first interacts with flash storage, which will also
answer how the bootrom is able to find our image in the first place.

The bootrom is the physically immutable boot code on the rp2350 that calls
into our bootloader. Before the bootrom can search flash storage for an image,
it must first figure out how to read the contents of flash storage, as there 
are a variety of flash storage implementations that might be used with the
rp2350 processor. The bootrom tries a variety of these QSPI "modes" to figure
out how to interact with flash and also to try to read the contents of flash
memory and find the program image. When the proper way to interface with flash
storage is identified, an "XIP Setup Function" is written to the first 256
bytes of boot RAM. From the rp2350 datasheet:

_"This setup code, referred to as an XIP setup function, is usually copied into
RAM before execution to avoid running from flash whilst the XIP interface is
being reconfigured."_

And, I suppose, to use a non-default XIP setup function, the datasheet says this:

_"You should save your XIP setup function in the first 256 bytes of boot RAM to
make it easily locatable when the XIP mode is re-initialised following a serial
flash programming operation which had to drop out of XIP mode. The bootrom
writes a default XIP setup function to this address before entering the flash
image, which restores the mode the bootrom discovered during flash programming."_ [3]

In any case, to initialize the XIP subsystem, we will need to _read_ and then find
a way to _execute_ the XIP setup function. To do this, we can simply push the
contents of the boot RAM to an executable location (the program stack), and invoke
it.

```
    /* BOOTRAM_BASE = 0x400e0000 */
    mv a0, sp 
    addi sp, sp, -256
    mv a1, sp
    la a2, 0x400e0000u
    
copy_xip_fn:
    lw a3, (a2)
    sw a3, (a1)
    addi a2, a2, 4
    addi a1, a1, 4
    bltu a1, a0, copy_xip_fn

    /* execute xip setup function */
    jalr sp
    addi sp, sp, 256
```

Running this doesn't appear to break anything, (and the function returns), but
it's not actually obvious what initialization is being performed for our board.
We set a breakpoint and inspect the boot RAM in `gdb` with the following
instruction:

```
x/64i 0x400e0000
```
>NOTE: (256 bytes / 4-word instructions = 64 instructions)

...which shows me the following:

```
   0x400e0000:  auipc   a2,0x0
   0x400e0004:  lw      a0,16(a2)
   0x400e0006:  lw      a1,20(a2)
   0x400e0008:  lw      a2,12(a2)
   0x400e000a:  jr      a2
   0x400e000c:  .insn   2, 0x7d7e
   0x400e000e:  unimp
   0x400e0010:  lb      zero,0(zero) # 0x0
   0x400e0014:  lb      zero,0(zero) # 0x0
   0x400e0018:  unimp
   0x400e001a:  unimp
   0x400e001c:  unimp
   ...etc
```

a2 is set to contain the auipc instruction's address. Then, a0, a1, and a2 get
the words at offsets 16, 20, and 12 bytes from a2 respectively (i.e. the words
encoded at 0x400e0010, 0x400e0014, 0x400e0000c) -- these appear as instructions
above, but are more informative as hex values:

```
(gdb) x/w 0x400e0010
0x400e0010:     0x00000003
(gdb) x/w 0x400e0014
0x400e0014:     0x00000003
(gdb) x/w 0x400e000c
0x400e000c:     0x00007d7e
```

Then, the assembler jumps to the address in a2 (0x7d7e). This is a point in the
bootrom, and is likely a helper function like those before it as detailed in
table 5.4.1 in the rp2350 datasheet [3]. With more digging I could probably
get more insight about what is configured here, but it's annoying, so I decided
instead to inspect the effect of this on some of the QSPI configuration registers.

12.14.2 goes into detail about the different kinds of transfers that occur over
QSPI. Among others, the transfers that occur are configured by  by the
M0/M1_RFMT and M0/M1_WFMT registers (including number of data lines), and the 
command (including prefix and suffix) are configure by the M0/M1_RCMD and
M0/M1_WCMD registers -- in both sets of registers, R indicates reads and W
writes.

Inspecting these registers gives the following:

```
M0_RFMT = 0x000492a8
M0_RCMD = 0x000000eb
M0_WFMT = 0x00001000
M0_WCMD = 0x0000a002
```

Studying the registers can inform us about how the QSPI has been configured.
For example, for reads on chip select (CSn) zero:
```
M0_RFMT
-------
DTR:            0x0 --> normal read command transfer rate
Dummy Len:      0x4 --> 16 dummy bits
Suffix Len:     0x2 --> 8 bit suffix
Prefix Len:     0x1 --> 8 bit prefix
Data Width:     0x2 --> Quad data transfer width
Dummy Width:    0x2 --> Quad dummy transfer width
Suffix Width:   0x2 --> Quad suffix transfer width
Address Width:  0x2 --> Quad address transfer width
Prefix Width:   0x0 --> Single prefix transfer width

M0_RCMD
-------
Prefix:         0xeb --> Command prefix, used since prefix len != 0
Suffix:         0x00 --> Command suffix, used since suffix len != 0
```

This gives us a glimpse into how transfers to/from memory are configured. To
be honest though, there are a lot of details about the memory transfer, and I
don't really have a deep understanding of what is going on here other than that
in theory using all data transfer lines should (hopefully) improve read
throughput, as opposed to the default (reset) values for M0_RFMT (0x1000) and
M0_RCMD (0xa003), which simply result in "03h" serial read transfers using one
data transfer line. It would make sense to revisit these settings if you have
tasks that result in large data transfers to/from flash. Perhaps we can test
alternate configurations against various benchmarks here later.

### .data Copy (11/29/24)

We must initialize `.data` section from flash storage into RAM. This is fairly
straightforward:

```
    la a0, __data_load_start
    la a1, __data_start
    la a2, __data_end
    
copy_data:
    beq a1, a2, copy_data_end
    lw a4, a3(a0)
    sw a4, a3(a1)
    addi a0, 4
    addi a1, 4
    j copy_data

copy_data_end:
    /* done */
```

### .bss Zero (11/29/24)

We must set the contents of `.bss` in RAM to 0. This is also fairly
straightforward.

```
    la a0, __bss_start
    la a1, __bss_end

zero_bss:
    beq a0, a1, zero_bss_end
    sw zero, (a0)
    addi a0, a0, 4
    j zero_bss

zero_bss_end:
    /* done */
```

### Blinky (11/30/24 - 12/1/24)

With this foundation, let's create a simple program. In a separate file
`main.S`, let's write blinky.

```
.section .text
.global blinky
blinky:
    ebreak
```

You can look at the pico 2 pinout document [4] to see what controls the on-board
LED. It is labeled GP25, for GPIO 25.

GPIO pins can be controlled by a variety of functions. For our purposes, we'll
just need Single-cycle IO (SIO) to control pin 25, and we only need to drive
an output signal to the LED to turn it on.

Firstly, it's necessary to configure the GPIO pin for SIO. To do this, we need
to use the `GPIO25_CTRL` register, offset from `IO_BANK0`. Inspecting the
documentation for this register shows that `0x5` in the least significant bits
selects the SIO function. For this register, it's fine to simply store that
value, as the other fields won't be used here:

```
    la a0, 0x400280cc   /* IO_BANK0 + GPIO25_CTRL */
    li a1, 0x5
    sw a1, (a0)
```

Pad control user bank at `0x40038000`. GPIO25 at offset `0x68` so `0x40038068`.
We can use this to clear the ISO control bit, which needs to be removed once
the pad is "configured by software" (that's us). Though in this case, we should
preserve the other bit values in the register. We can use the RISC-V B (bit
manipulation) extension for this purpose:

```
    la a0, 0x40038068   /* PADS_BANK0 + GPIO25 */
    lw a1, (a0)
    bclr a1, a1, 8
    sw a1, (a0)
```

Now we should be ready to use SIO to control the LED. We can enable GPIO output
on particular registers using a mask, in this case we will use a mask (25th bit)
when we write values to these registers. First, we must enable output for our
pin, which we can do with `GPIO_OE_SET`:

```
    la a0, 0xd0000038   /* SIO: GPIO_OE_SET */
    bset a1, zero, 25   /* Mask for GPIO pin 25 */
    sw a1, (a0)
```

Finally, we're ready to make our LED blink. We can set and clear the output to
our LED with the `GPIO_OUT_SET` and `GPIO_OUT_CLR` registers. For now, we can
spin in some simple loops to create a delay so that we may observe our blinky
LED. Here's what the loop looks like:

```
loop:
    sw a1, (a0)

    li a4, 0x200000
    mv a5, zero
spin1:
    addi a5, a5, 1
    blt a5, a4, spin1

    sw a1, (a2)

    li a4, 0x200000
    mv a5, zero
spin2:
    addi a5, a5, 1
    blt a5, a4, spin2

    j loop
```

Finally, we can put it all together:

```
.section .text
.global blinky
blinky:
    la a0, 0x400280cc   /* IO_BANK0: GPIO25_CTRL */
    li a1, 0x5
    sw a1, (a0)

    la a0, 0x40038068   /* PADS_BANK0 + GPIO25 */
    lw a1, (a0)
    bclr a1, a1, 8
    sw a1, (a0)

    la a0, 0xd0000038   /* SIO: GPIO_OE_SET */
    bset a1, zero, 25   /* Mask for GPIO pin 25 */
    sw a1, (a0)

    la a0, 0xd0000018   /* SIO: GPIO_OUT_SET */
    la a2, 0xd0000020   /* SIO: GPIO_OUT_CLR */
    
loop:
    sw a1, (a0)

    li a4, 0x200000
    mv a5, zero
spin1:
    addi a5, a5, 1
    blt a5, a4, spin1

    sw a1, (a2)

    li a4, 0x200000
    mv a5, zero
spin2:
    addi a5, a5, 1
    blt a5, a4, spin2

    j loop
```

It's worth also taking a quick peek at how our implementation compares to the
blink_simple example from the `pico-examples` repository. If we disassemble the
output we can see some interesting things. For example, in `gpio_init`:

```
gpio_init:
    # NOTE: a0 gets 25

    # 1.
    bset a3,zero,a0
    lui	a4,0xd0000
    lui	a5,0x40038
    sw	a3,64(a4)

    addi a5,a5,4 # 40038004 <__StackTop+0x1ffb6004>
    sw a3,32(a4)

    # 2.
    sh2add a5,a0,a5
    lw a4,0(a5)
    lui	a3,0x1
    add	a3,a3,a5
    xori a4,a4,64
    andi a4,a4,192
    lui	a2,0x40028
    sw a4,0(a3) # ???

    sh3add a0,a0,a2
    li a4,5
    lui	a3,0x3
    sw a4,4(a0)

    # 3.
    add	a5,a5,a3
    li a4,256
    sw a4,0(a5) # ???
```

Regarding 1: the SDK makes sure to clear the output bits, which is good
practice. If we wanted to make our implementation more robust (I don't care to)
we should include that so that we know more precisely the state of the system
before we start sending signals willy-nilly.

Regarding 2: if you go through the effort to figure out the value in `a3` at
the time the store instruction is executed, you will find that it is at 
`0x40039068`. But this doesn't match any registers in the rp2350 data sheet
offset from `PADS_BANK0` -- it is `0x1000` offset from the `GPIO25` register.
And at the end of 3, a value is stored at `0x4003b068` -- which is `0x3000`
off of this register. What is going on here?

The answer is that these are atomic registers, detailed in 2.1.3 in the rp2350
datasheet [3]. For `PADS_BANK0` (and other peripherals/registers), there are
corresponding "atomic" registers that let you perform atomic XORs on write
(+ 0x1000), atomic bitmasks on write (+ 0x2000), and atomic bitmask clears on
write (+ 0x3000). This is how the SDK clears the ISO control bit for GPIO25,
rather than performing separate (not atmoic) loads and stores to preserve the
value when attempting to clear the bit, for example. As with 1, we should
incorporate these registers when appropriate for a robust (correct) system, 
especially when we start adding multiple tasks (and making use of our second
core!), where race conditions might occur.

### Project Layout (12/13/24 - 12/15/24)

Now that we've gotten started (and we're really about to hit our stride here),
it would be wise to set ourselves up for the future by implementing some good
software engineering practices. Let's revisit our Makefile, directory structure,
and workflow so that we are set up for the future as our project becomes more
complex. We'll probably want to be able to do the following:

- Easily compile and run different programs (OSes, applications, tests, etc.)
- Easily add new header and source files which get linked in during compilation
- Automate boring stuff where possible (e.g. code formatting)
- Write and maintain documentation

So where do we start? Let's begin by organizing our code in such a way that we
can easily add new `.c` and `.h` files. Let's also make it so that we can
compile and run a variety of "user programs" (applications) on top of our
kernel. We'll add the following directories:

```
kernel
user
```

While we're at it, let's move build artifacts out of the top level directory. 
Let's put our executables in `build/`. To support these changes as well as 
compiling different and arbitrary programs, we'll need to modify our Makefile.
Since we want to support multiple build targets, let's add the following:

```
APP ?= blinky
TARGET := build/$(APP).elf
```

This way, when we run make we can specify the target application e.g.

```
make run APP=blinky
```

Let's specify where we get our sources, and where we'll build our objects
before linking:

```
BUILD_DIR := build

KERNEL_DIR := kernel
KERNEL_SRCS := $(wildcard $(KERNEL_DIR)/*.c $(KERNEL_DIR)/*.S)
KERNEL_OBJ := $(BUILD_DIR)/kernel.o

USER_DIR := user/$(APP)
USER_SRCS := $(wildcard $(USER_DIR)/*.c $(USER_DIR)/*.S)
USER_OBJ := $(BUILD_DIR)/user_$(APP).o
```

Now let's create the rules to build our `kernel` and `user` objects:

```
$(KERNEL_OBJ): $(KERNEL_SRCS) $(KERNEL_INCS)
	$(CC) $(CFLAGS) -I $(KERNEL_DIR) -c -o $@ $(KERNEL_SRCS)

$(USER_OBJ): $(USER_SRCS) $(USER_INCS)
	$(CC) $(CFLAGS) -I $(USER_DIR) -c -o $@ $(USER_SRCS)
```

Then finally the rule to actually link the objects to build the executable.
As a reminder, these are the flags we use for linking (with the linker script):

```
MEMMAP := memmap.ld

LDFLAGS = -T $(MEMMAP) -e _entry_point -Wl,--no-warn-rwx-segments
```

So our final rule for building the executable is:

```
$(TARGET): $(KERNEL_OBJ) $(USER_OBJ) $(MEMMAP)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET)  $(KERNEL_OBJ) $(USER_OBJ)
```

Since we'll support running different executables, we'll need to change how our
inital gdb script works, as it should be using the executable built with the app
we choose when running `make run APP=whatever`. Let's add an easily identifiable
(replaceable) string `<PROGRAM>` so that it looks looks like this:

```
target remote localhost:3333
monitor program <PROGRAM>
monitor reset init
continue

define ic
    set $pc = $pc + 0x4
    continue
end
```

We'll stick it in a catch-all directory `util`.
Then, we can modify our Makefile to generate the real script in our `run` rule:

```
GDB_TEMPLATE := util/gdb_template

run: $(TARGET) $(GDB_TEMPLATE)
	sed "s|<PROGRAM>|$(TARGET)|" $(GDB_TEMPLATE) > init.gdb
	$(GDB) $(TARGET) -x init.gdb
```

These changes make it so that we compile all of our source code in `kernel/` to
`build/kernel.o`, and all our application code in `user/appname/` to
`build/user_appname.o`. This has the advantage of letting us compile these two
pieces independently, but it's not yet ideal for large projects. We may later
improve the makefile to allow each kernel and user module to be compiled
independently to optimize the build process, but when we do so we'll also have
to start worrying about things like module name collisions and hashing.
It feels like overkill now since we only have two files, and not even a proper
separation between kernel and userspace. So I'll defer this effort for now.

We can automate the formatting of our C source code using `clang-format` and
git pre-commit hooks. Here's the boring but effective style I prefer:

```
BasedOnStyle: LLVM
IndentWidth: 4
TabWidth: 4
UseTab: Never
AlignAfterOpenBracket: Align
AlignConsecutiveMacros: true
AllowShortFunctionsOnASingleLine: false
ColumnLimit: 80
```

You can go ahead and change it if you are the kind of weirdo who likes 2-space
indents or something.

Here's the `.pre-commit-config.yaml`:

```
repos:
- repo: https://github.com/pre-commit/mirrors-clang-format
  rev: v14.0.6
  hooks:
  - id: clang-format
```

...and you can install the pre-commit hook with `pre-commit install`.

To set up documentation generation, we'll use `doxygen`.

```
sudo apt install doxygen graphviz
```

You can generate a doxygen configuration file with the following:

```
doxygen -g
```

Configure it how you like. Notably, we'll set `OUTPUT_DIRECTORY = docs`. Then
let's move it to `util/` and add a rule for it to our Makefile:

```
docs:
    doxygen util/Doxyfile
```

You can also add it to the clean rule, `.PHONY`, yada yada.

Let's add some stuff to our `.gitignore` file to ensure we're not always
pushing extra crud to the .github repo:

```
# Doxygen generated files
docs/html/*
docs/latex/*
docs/xml/*
docs/*.idx

# Build artifacts
build/*
```

Finally, let's create a `README.md` with instructions for running and building
our project:

```
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
configure it for debugging the pico 2 with `make console`.

Then, in a separate window, compile and run the user application (e.g. for
blinky): `make run APP=blinky`
```

Congrats, you've gone from hobbyist developer to professional software engineer.
That's basically all there is to it.

### Exception & Interrupt Handling (12/16/24 - 12/21/24)

We still haven't implemented "trap" handling, we only created a vector table
with unimplemented interrupt and exception handlers. From `kernel/startup.S`:

```
/**
 * Note that in 'Vectored' mode, the vector table must be 64 byte aligned.
 * See rp2350 datasheet section 3.8.4.2.1
 * See riscv-privileged-20211203 section 3.1.7
 */
.section .vectors, "ax"
__vector_table:

/* disable compressed instructions */
.option push
.option norvc
.option norelax

j isr_exc      /* BASE +  0 */ 
.word 0        /* BASE +  4 */ 
.word 0        /* BASE +  8 */ 
j isr_msi      /* BASE + 12 */ 
.word 0        /* BASE + 16 */ 
.word 0        /* BASE + 20 */ 
.word 0        /* BASE + 24 */ 
j isr_mti      /* BASE + 28 */ 
.word 0        /* BASE + 32 */ 
.word 0        /* BASE + 36 */ 
.word 0        /* BASE + 40 */ 
j isr_mei      /* BASE + 44 */ 
 
.option pop

.weak isr_exc
isr_exc:
    ebreak
    j jail

.weak isr_msi
isr_msi:
    ebreak
    j jail

.weak isr_mti
isr_mti:
    ebreak
    j jail

.weak isr_mei
isr_mei:
    ebreak
    j jail
```

Events in Operating Systems (including RTOSes) are primarily driven by
"traps", which is a catch-all term for exceptions and interrupts in RISC-V
jargon. Exceptions (`isr_exc`) are triggered by faults such as illegal
operations (e.g. memory access violations). Software interrupts (`isr_msi`)
are propogated by software for various reasons, such as user space programs
requesting privileged services to be carried out by the kernel (system calls).
Timer interrupts (`isr_mti`) are used for clocks, which are important for
implementing time-sensitive tasks and operations. External interrupts (`isr_mei`)
indicate that events outside the processor (e.g. DMA transfers, peripheral
events) have transpired, and enable the operating system to react accordingly.

All of our trap handling implementation will be informed by the RISC-V
specification [1], but the rp2350 datasheet also details the Hazard3 interrupt
controller and associated Xh3irq extension for handling external interrupts,
so we'll also refer to that later.

#### Synchronous Exception Handling

Firstly, let's consider synchronous exceptions. From the RISC-V privileged spec,
we know that synchronous exceptions jump to the base address of our vector
table, but once we're in the interrupt service routine (`isr_exc`), how can
we distinguish the cause of our exception?

Even though exceptions don't vector into the offset set by `mcause` to execute
a corresponding `isr`, `mcause` is still set with an exception code indicating
the cause of the exception. In the privileged spec 3.1.15, we can see a list of
exception codes for exceptions (interrupt bit is 0!) in table 3.6:

| Interrupt Bit | Exception Code | Description |
| ---             | ---            | ---         |
| 0 | 0 | Instruction address misaligned |
| 0 | 1 | Instruction access fault |
| 0 | 2 | Illegal instruction |
| 0 | 3 | Breakpoint |
| 0 | 4 | Load address misaligned |
| 0 | 5 | Load access fault |
| 0 | 6 | Store/AMO address misaligned |
| 0 | 7 | Store/AMO access fault |
| 0 | 8 | Environment call from U-mode |
| 0 | 9 | Environment call from S-mode |
| 0 | 10 | Reserved |
| 0 | 11 | Environment call from M-mode |
| 0 | 12 | Instruction page fault |
| 0 | 13 | Load page fault |
| 0 | 14 | Reserved |
| 0 | 15 | Store/AMO page fault |
| 0 | 16-23 | Reserved |
| 0 | 24-31 | Designated for custom use |
| 0 | 32-47 | Reserved |
| 0 | 48-63 | Designated for custom use |
| 0 | >= 64 | Reserved |

The RISC-V privileged spec indicates that other information (such as faulting
virtual memory address) _may_ be written to the `mtval`, but the rp2350
datasheet indicates that Hazard3 hardwires this register to 0, so it will not
provide us any useful information (see 3.8.4.1 in [1]).

The `mepc` register is also of interest, as it provides the address of the
instruction that was interrupted or encountered the exception.

The `mtinst` register may in some cases be written with a value that provides
information about the trapping instruction according to 8.4.5 [3]. But it 
would seem that writing to this register is not strictly necessary, and might
just be implemented by hardware. Since the rp2350 datasheet makes no mention
of this register, I'll assume it's not applicable here.

The `mscratch` register is _"dedicated for use by machine mode... it is used to
hold a pointer to a machine-mode hart-local context space and swapped with a
user register upon entry to an M-mode trap handler."_ [3]. A pointer to a
machine-mode hart-local context space... for what, exactly? Apparently, for
whatever we want. Hence the name scratch. The rp2350 datasheet suggests it may 
be used to store some pointer to a dedicated interrupt handler stack. The pico
sdk uses it to check for nested exceptions by zeroing it out outside the
exception handler (during boot and when returning from exception handling) and
using it to save the return address inside the handler, which has the added
benefit of preventing the exception handler from needing to push the return
address to the stack before calling the appropriate exception handler. It
accomplishes this with a simultaneous read and write to swap the contents of
`ra` and `mstatus`. This seems like a wise idea, so let's also use the
`mscratch` register for this purpose. If we find a better design, it should be
easy enough to fix. At the beginning of our startup code:

```
    csrw mscratch, zero
```

And we'll do the other part in the exception handling. There's one more part 
to look at before that though, and that is the calling convention. If we want
to call subroutines to handle interrupts, we'll need to obey the RISC-V calling
conventions. We should also note that unlike other architectures RISC-V doesn't
do much work for us in this regard, the rp2350 datasheet says this:

_"Hardware... does not save the core GPRs, and software is responsible for
ensuring the execution of the handler does not perturb the foreground
context"_ [1].

With that, let's create a simple exception handler that checks for nested
exceptions, saves/restores caller-saved registers, and dispatches exception
requests to the appropriate request handler. Here's what I came up with:

```
/**
 * @brief Handles synchronous exceptions as defined by the RISC-V ISA.
 * Assumes mscratch is zeroed out on startup, and the register is used
 * to detect nested exceptions, although in that case the core is halted.
 */
isr_exc:
    // swap mscratch and ra to detect nested exceptions.
    // if mscratch (swapped to ra) not zero, we just
    // nested an exception, and should go to jail.
    csrrw ra, mscratch, ra
    bnez ra, jail
    addi sp, sp, -60
    // save the remaining caller-saved registers before dispatch
    sw a0, 0(sp)
    sw a1, 4(sp)
    sw a2, 8(sp)
    sw a3, 12(sp)
    sw a4, 16(sp)
    sw a5, 20(sp)
    sw a6, 24(sp)
    sw a7, 28(sp)
    sw t0, 32(sp)
    sw t1, 36(sp)
    sw t2, 40(sp)
    sw t3, 44(sp)
    sw t4, 48(sp)
    sw t5, 52(sp)
    sw t6, 56(sp)
    
    // dispatch to correct exception handler
    csrr t0, mcause
    li t1, 11
    bltu t1, t0, isr_unhandled_exc

    la t1, __exception_table
    sh2add t0, t0, t1
    lw t0, (t0)
    jalr t0

    // restore caller-saved registers
    lw t6, 56(sp)
    lw t5, 52(sp)
    lw t4, 48(sp)
    lw t3, 44(sp)
    lw t2, 40(sp)
    lw t1, 36(sp)
    lw t0, 32(sp)
    lw a7, 28(sp)
    lw a6, 24(sp)
    lw a5, 20(sp)
    lw a4, 16(sp)
    lw a3, 12(sp)
    lw a2, 8(sp)
    lw a1, 4(sp)
    lw a0, 0(sp)

    addi sp, sp, 60
    // restore ra and clear mscratch
    csrrw ra, mscratch, zero
    mret
```

Also defined is the exception table, containing addresses of exception handlers:

```
.p2align 2
.global __exception_table
__exception_table:
.word isr_inst_align_exc        // mcause = 0
.word isr_inst_access_exc       // mcause = 1
.word isr_inst_illegal_exc      // mcause = 2
.word isr_inst_ebreak_exc       // mcause = 3
.word isr_load_align_exc        // mcause = 4
.word isr_load_access_exc       // mcause = 5
.word isr_store_align_exc       // mcause = 6
.word isr_store_access_exc      // mcause = 7
.word isr_env_umode_exc         // mcause = 8
.word isr_env_smode_exc         // mcause = 9
.word isr_unhandled_exc         // mcause = 10 (reserved)
.word isr_env_mmode_exc         // mcause = 11
// NOTE: mcause > 11 should be isr_unhandled_exc
```

And finally a macro plus a weak definition of the corresponding handlers:

```
.macro weak_def name
.weak \name
\name:
    ebreak
.endm

weak_def isr_inst_align_exc
weak_def isr_inst_access_exc
weak_def isr_inst_illegal_exc
weak_def isr_inst_ebreak_exc
weak_def isr_load_align_exc
weak_def isr_load_access_exc
weak_def isr_store_align_exc
weak_def isr_store_access_exc
weak_def isr_env_umode_exc
weak_def isr_env_smode_exc
weak_def isr_env_mmode_exc

.global isr_unhandled_exc
isr_unhandled_exc:
    ebreak
    // for now, go to jail
    j jail
```

Assuming our debugger doesn't get messed up, this should let us break at the
handler for the appropriate exception. Later, when we actually want to handle
the exceptions, we can override this definition. But for now, we can test that
the appropriate exception handler is reached. In a new "user" project, write
a program that attempts to load from a misaligned address:

```c
unsigned long load_align_fault();
int main() {
    load_align_fault();
    return 0;
}

unsigned long load_align_fault() {
    return *(unsigned long *)0x10000001;
}
```

Then, run `make run APP=test_exception` (or whatever app name you chose). In
gdb, you should end up in the appropriate spot:

```
Thread 1 "rp2350.dap.core0" received signal SIGTRAP, Trace/breakpoint trap.
isr_load_align_exc () at kernel/startup.S:289
289     weak_def isr_load_align_exc
```

I included a few other simple tests in `user/test_exception/main.c`.

#### Software Interrupt Handling

We must also handle machine software interrupts (`isr_msi`). The RISC-V
privileged isa isn't very explicit about what software interrupts are used for,
but in discussions of interrupt priorities it is implied that they are used
for inter-processor messaging. From the aside at the end of 3.1.9:

_"Software interrupts are handled before internal timer interrupts, because
internal timer interrupts are usually intended for time slicing, where time
precision is less important, whereas software interrupts are used for 
inter-process messaging."_ [3]

From the rp2350 datasheet section 3.8.4.2.1, we can see specifically how
`mip.msip` is used:

_"The standard software interrupt MIP.MSIP connects to the RISCV_SOFTIRQ
register in the SIO subsystem. The register has a single bit per hart, which
asserts the soft IRQ interrupt to that hart. This can be used to interrupt the
other hart, or to interrupt yourself as though the other hart had interrupted
you, which can help to make handler code more symmetric. On RP2350 there is a
one-to-one correspondence between harts and cores, so you could equivalently
say there is one soft IRQ per core."_ [3]

So `isr_msi` will fire when the appropriate bit is set in the RISCV_SOFTIRQ
control register, and it is designed primarily for cores to interrupt one
another, although we write the software and thus ultimately decide the
conditions under which these interrupts occur. Hence software interrupt.

It wouldn't appear that there's much unique work to be done here yet, but we
can at least set up a generic interrupt handler that dispatches to a weak
definition of an isr so that we know things are working as expected. 

```
isr_msi:
    // push caller-saved
    addi sp, sp, -64
    sw ra, 0(sp)
    sw a0, 4(sp)
    sw a1, 8(sp)
    sw a2, 12(sp)
    sw a3, 16(sp)
    sw a4, 20(sp)
    sw a5, 24(sp)
    sw a6, 28(sp)
    sw a7, 32(sp)
    sw t0, 36(sp)
    sw t1, 40(sp)
    sw t2, 44(sp)
    sw t3, 48(sp)
    sw t4, 52(sp)
    sw t5, 56(sp)
    sw t6, 60(sp)

    jal isr_soft_irq

    // restore caller-saved
    lw t6, 60(sp)
    lw t5, 56(sp)
    lw t4, 52(sp)
    lw t3, 48(sp)
    lw t2, 44(sp)
    lw t1, 40(sp)
    lw t0, 36(sp)
    lw a7, 32(sp)
    lw a6, 28(sp)
    lw a5, 24(sp)
    lw a4, 20(sp)
    lw a3, 16(sp)
    lw a2, 12(sp)
    lw a1, 8(sp)
    lw a0, 4(sp)
    lw ra, 0(sp)
    addi sp, sp, 64

    mret
```

And a weak definition for `isr_soft_irq`:

```
weak_def isr_soft_irq
```

Since we're obeying the calling convention, we can later define this function
as a C handler if we so please, just like the exception isrs. Although we
haven't initialized core1 yet, we can still pend an interrupt on ourselves from
core0 to ensure the handler gets executed as expected. Note that when we
want to initialize a multi-processor runtime, we'll need to enable this software
interrupt via `mie.msie` as shown in this test.

```
test_msi:
    // enable software interrupts (msie = 0x8 in mie)
    csrsi mie, 0x8u

    // Set the appropriate bit in RISCV_SOFTIRQ
    // SIO_BASE                 = 0xd0000000
    // RISCV_SOFTIRQ Offset     = 0x1a0
    li a1, 0xd00001a0 
    li a2, 1
    sw a2, 0(a1)

    fence.i
    j jail
```

If we jump to `test_msi` somewhere after our setup, we can see that we land in
the software interrupt isr in gdb:

```
Thread 1 "rp2350.dap.core0" received signal SIGTRAP, Trace/breakpoint trap.
isr_soft_irq () at kernel/startup.S:372
372     weak_def isr_soft_irq
```

#### Timer Interrupt Handling

We must also handle timer interrupts (`isr_mti`). Here's what the RISC-V
privileged ISA has to say about timer interrupts:

This shouldn't be too different from the software interrupt. We'll follow
the same procedure and define a weak handler so that we can decide what to do
later by defining a c handler for timer interrupts. 

```
isr_mti:
    // push caller-saved
    addi sp, sp, -64
    sw ra, 0(sp)
    sw a0, 4(sp)
    sw a1, 8(sp)
    sw a2, 12(sp)
    sw a3, 16(sp)
    sw a4, 20(sp)
    sw a5, 24(sp)
    sw a6, 28(sp)
    sw a7, 32(sp)
    sw t0, 36(sp)
    sw t1, 40(sp)
    sw t2, 44(sp)
    sw t3, 48(sp)
    sw t4, 52(sp)
    sw t5, 56(sp)
    sw t6, 60(sp)

    jal isr_mtimer_irq

    // restore caller-saved
    lw t6, 60(sp)
    lw t5, 56(sp)
    lw t4, 52(sp)
    lw t3, 48(sp)
    lw t2, 44(sp)
    lw t1, 40(sp)
    lw t0, 36(sp)
    lw a7, 32(sp)
    lw a6, 28(sp)
    lw a5, 24(sp)
    lw a4, 20(sp)
    lw a3, 16(sp)
    lw a2, 12(sp)
    lw a1, 8(sp)
    lw a0, 4(sp)
    lw ra, 0(sp)
    addi sp, sp, 64

    mret

// ...

weak_def isr_mtimer_irq
```

Note that for the RISC-V architecture's timer registers `mtime` and `mtimecmp`,
these hold 64-bit values even in 32 bit architectures. Thus they are implemented
by multiple registers in the rp2350.

For completeness (and so that we understand how the RISC-V architecture works)
let's still write a test for our implementation. From the rp2350 spec on
writing new timer comparison values:

_"Use the following sequence to write a new 64-bit timer comparison value
without causing spurious interrupts:
1. Write all-ones to MTIMECMP (guaranteed greater than or equal to the old value, and the lower half of the target value).
2. Write the upper half of the target value to MTIMECMPH(combined 64-bit value is still greater than or equal to the target value).
3. Write the lower half of the target value to MTIMECMP.
The RISC-V timer can count either ticks from the system-level tick generator...
or system clock cycles, selected by the MTIME_CTRL register. Use a 1 microsecond
time base for compatibility with most RISC_V software."_

So our test can look something like this:

```
test_mti:
    // enable timer interrupts (mtie = 0x80)
    li a0, 0x80u
    csrs mie, a0

    // set mtime lower and higher half to 0
    li a0, 0xd00001b0
    sw zero, 0(a0) // MTIME
    sw zero, 4(a0) // MTIMEH

    
    // 1. write all ones to MTIMECMP
    li a0, 0xd00001b8 // MTIMECMP
    li a1, 0xffffffff // reset value
    sw a1, (a0)

    // 2. write value to MTIMECMPH
    li a0, 0xd00001bc // MTIMECMPH
    sw zero, (a0)

    // 3. write value to MTIMECMP
    li a0, 0xd00001b8 // MTIMECMP
    li a1, 1
    sw a1, (a0)

    // enable the timer with MTIME_CTRL, set fullspeed with bit 1
    li a0, 0xd00001a4
    li a1, 0x3u
    sw a1, 0(a0)

    fence.i
    j jail
```

#### External Interrupt Handling

We must also handle timer interrupts (`isr_mei`). Here's what the RISC-V
privileged ISA has to say about external interrupts:

The Xh3irq is an **extension** that is _"architected as a layer on top of the
standard `mip.meip` external interrupt line"_ [3]. Recall from the RISC-V spec
that `mip.meip` represents the machine external interrupt pending vector. Thus
what follows relates to the handling of "machine external interrupts", and does
not affect timer and software interrupts.

The Xh3irq interrupt controller documentation provides a series of control
status registers intended to support programmable, priority-based interrupts.
The hardware implements "pre-emption priority logic" but the software still
dictates how interrupts received via `mip.meip` are dispatched (like the other 
kinds of interrupts we've been handling). A brief overview of the registers is
provided at the beginning of 3.8.6.1:

```
- MEIEA: external interrupt enable array
- MEIPA: external interrupt pending array
- MEIFA: external interrupt force array
- MEIPRA: external interrupt priority array
- MEINEXT: get next external interrupt
- MEICONTEXT: external interrupt context register
```

The "A" suffix in the first four registers represents array -- writes/reads to
these registers may be used to read/write positions within a 512 bit vector
representing the enabled, pending, and forced interrupts respectively. The array
can be thought of as 32 separate slices, each 16 bits, where each bit represents
one of the 512 priorities of external interrupt. When using one of the registers
with the "A" suffix, bits [15:0] represent the index of the 16-bit slice and
[31:16] represent the contents of the array. To write a 1 to bit 0, for example,
you would write `0x00010000`. Let's say we wanted to enable external interrupt
number 288. Then, in c: 

```
uint32_t lower = (288 / 16);                    // gives index
uint32_t upper = 1 << (288 % 16);               // gives offset into slice
*((uint32_t *)0xbe0) = (upper << 16) | lower;   // write 32 bit content to MEIEA
```

While the interrupt controller appears to support as many as 512 external
interrupt lines the rp2350 datasheet says that "RP2350 configures Hazard3
with the Xh3irq interrupt controller, with 52 external interrupt lines and
16 levels of pre-emption priority." [3]. Thus we should to use only indices
0 - 3 to get the first four 16-bit windows to cover all external interrupts.
IRQ numbers 0 through 51 are documented in the rp2350 datasheet section 3.2.

> NOTE: the rp2350 datasheet and the SDK sometimes also refer to machine
external interrupts as "system interrupts" for some reason. Although, I'm not
sure whether this maps onto our machine external interrupts 1:1 or if system
interrupts also encompasses other kinds of interrupts (machine software
interrupts, machine timer interrupts). 

Two more registers of interest may be used to see the current interrupt
handling context, plus get the next highest-priority pending machine external
interrupt. These registers are `MEICONTEXT` and `MEINEXT`. 

`MEICONTEXT` appears to be useful for storing state in case we want to enable
pre-emption during our system interrupt handling. It contains the information 
on what the current context is (which IRQ is being serviced), plus which
interrupt is being preempted, if any.

If we want to use it properly, and enable pre-emption, we'll  need to push it
onto the stack just like our other caller-saved registers, so that it doesn't
get trashed if it get's pre-empted by a higher priority interrupt.

`MEINEXT` is useful for at least two purposes: one is that we can service 
machine external interrupts in a priority order by getting the next pending
interrupt (if any) from this register, and two is that we can use this within
our generic mei handler to get the next interrupt if it exists before
returning, thus potentially letting us do interrupt tail calls and eliminating
the need to repeatedly save and restore context.

> **Question**: what is done for us by hardware upon receiving a machine external
> interrupt?
> **Answer**: See [3] 3.8.6.1.5 for how MEICONTEXT is automatically updated
>             See [3] 3.8.4 Interrupts and Exceptions trap entry sequence

The datasheet actually provides a minimal handler implementation without
preemption in 3.8.6.1.6. We can use it as a basis for our first draft:

```
isr_mei:
    // NOTE: mstatus.mie cleared by hardware, disabling preemption
    // push caller-saved
    addi sp, sp, -64
    sw ra, 0(sp)
    sw t0, 4(sp)
    sw t1, 8(sp)
    sw t2, 12(sp)
    sw a0, 16(sp)
    sw a1, 20(sp)
    sw a2, 24(sp)
    sw a3, 28(sp)
    sw a4, 32(sp)
    sw a5, 36(sp)
    sw a6, 40(sp)
    sw a7, 44(sp)
    sw t3, 48(sp)
    sw t4, 52(sp)
    sw t5, 56(sp)
    sw t6, 60(sp)
get_first_irq:
    // read next highest-priority active IRQ (<< 2) from MEINEXT
    csrr a0, 0xbe4
    // MSB will be set if there is no active IRQ at the current priority level
    bltz a0, no_more_irqs
dispatch_irq:
    lui a1, %hi(__external_interrupt_table)
    add a1, a1, a0
    lw a1, %lo(__external_interrupt_table)(a1)
    jalr ra, a1
get_next_irq:
    // read next-highest-priority IRQ
    csrr a0, 0xbe4
    // MSB will be set if there is no active IRQ at the current priority level
    bgez a0, dispatch_irq
no_more_irqs:
    // restore caller-saved
    lw t6, 60(sp)
    lw t5, 56(sp)
    lw t4, 52(sp)
    lw t3, 48(sp)
    lw t2, 44(sp)
    lw t1, 40(sp)
    lw t0, 36(sp)
    lw a7, 32(sp)
    lw a6, 28(sp)
    lw a5, 24(sp)
    lw a4, 20(sp)
    lw a3, 16(sp)
    lw a2, 12(sp)
    lw a1, 8(sp)
    lw a0, 4(sp)
    lw ra, 0(sp)
    addi sp, sp, 64
    mret
```

Let's write two quick tests to make sure we understand how to use teh sliding
window for the array registers, plus to ensure we're dispatching everything
correctly. We'll write one test for IRQ 0 and one for IRQ 44:

```
/**
 * @brief Tests external interrupts (cause 11, IRQ 0)
 */
test_mei_irq0:
    /* First, enable the corresponding mei IRQ number, let's say IRQ 0
     * 
     * MEIEA Offset: 0x00000be0
     * Bit key for MEIEA: W=Window, R=Reserved, I=Index
     * 
     * WWWW WWWW WWWW WWWW RRRR RRRR RRRI IIII
     *
     * So, to set IRQ 0, we need to set the rightmost W to 1.
     * 
     * 0000 0000 0000 0001 0000 0000 0000 0000
     *
     * Which equals 0x00010000, so let us write this content to MEIEA.
     * This will _enable_ interrupt request number 0, which is a machine
     * external interrupt.
     */
    li a0, 0x10000
    csrw 0xbe0, a0

    /* Next, we can create the interrupt request using MEIFA.
     *
     * MEIFA Offset: 0x00000be2
     * 
     * The register works the same as MEIA, except this time writing the
     * literal will force an interrupt.
     */
    csrw 0xbe2, a0
    fence.i

    // We shouldn't make it this far, so go to jail
    j jail


/**
 * @brief Tests external interrupts (cause 11, IRQ 44)
 */
test_mei_irq44:
    /* MEIEA Offset: 0x00000be0
     * Bit key for MEIEA: W=Window, R=Reserved, I=Index
     * 
     * WWWW WWWW WWWW WWWW RRRR RRRR RRRI IIII
     * 1000 0000 0000 0000 0000 0000 0000 0010
     */
    li a0, 0x10000002
    csrw 0xbe0, a0

    // now set MEIFA, blah blah blah
    csrw 0xbe2, a0
    fence.i

    j jail
```

Great, that appears to work.

Now let's implement preemption. There are some subtleties to what follows, and 
to be clear I adapted this implementation from the pico-sdk. In the pico sdk
repo, you can check out `src/rp2_common/pico_crt0/crt0_riscv.S` for the original
code. Nonetheless, I will rewrite it here, explain it, and then test it.

Firstly, the hardware automatically makes changes to `mepc` and `mstatus` upon
trap entry (see 3.8.4 Interrupts and Exceptions trap entry sequence). So these
let's save these by pushing them onto the stack, just like the other
caller-saved registers:

```
    csrr a0, mepc
    csrr a1, mstatus
    sw a0, 64(sp)
    sw a1, 68(sp)
```

We must also remember that the priority of the currently handled IRQ, the
priority of an IRQ required to preempt the current IRQ, are represented and
controlled by MEICONTEXT. Additionally, the rp2350 datasheet makes this 
recommendation in 3.8.6.1.5:

_"To avoid awkward interactions between the MIP.MEIP handler, which should be
aware of the Xh3irq extension, and the MTIP/MSIP handlers, which may not be,
it's best to avoid pre-emption of the former by the latter. MEICONTEXT.CLEARTS,
MTIESAVE and MSIESAVE support disabling and restoring the timer/software
interrupt enables as part of the MEICONTEXT CSR accesses that take place during
context save/restore in the MEIP handler."_ [3]

In one instruction, we can save the MITE and MSIE values in our current context
to MEICONTEXT, read MEICONTEXT to a register (that we will push onto the stack),
and also clear the machine timer and machine software interrupt enable
registers to prevent those interrupts from preempting our handler. Then, we can
push the saved context onto the stack:

```
save_meicontext:
    csrrsi a2, 0xbe5, 0x1 // MEICONTEXT, MEICONTEXT.CLEARTS bits
    sw a2, 72(sp)
```

Before servicing interrupt requests, we can now enable preemption by setting the
global `mstatus.mie` within the interrupt handler:

```
   csrsi mstatus, 0x8
```

And then disable it while we loop looking for other IRQs to service:

```
    csrci mstatus, 0x8
    j get_next_irq
```

Finally, when we exit, we must remember to restore the MEICONTEXT, mstatus,
and mepc registers. All together, my implementation of `isr_mei` looks like this:

```
/**
 * @brief Handles machine external interrupts without preemption.
 */
isr_mei:
    // NOTE: mstatus.mie automatically cleared by hardware, disabling preemption
    // push caller-saved
    addi sp, sp, -76
    sw ra, 0(sp)
    sw t0, 4(sp)
    sw t1, 8(sp)
    sw t2, 12(sp)
    sw a0, 16(sp)
    sw a1, 20(sp)
    sw a2, 24(sp)
    sw a3, 28(sp)
    sw a4, 32(sp)
    sw a5, 36(sp)
    sw a6, 40(sp)
    sw a7, 44(sp)
    sw t3, 48(sp)
    sw t4, 52(sp)
    sw t5, 56(sp)
    sw t6, 60(sp)

    csrr a0, mepc
    csrr a1, mstatus
    sw a0, 64(sp)
    sw a1, 68(sp)

save_meicontext:
    csrrsi a2, 0xbe5, 0x1 // MEICONTEXT, MEICONTEXT.CLEARTS bits
    sw a2, 72(sp)

get_next_irq:
    // reads next highest priority (IRQ << 2) from MEINEXT into a0 AND
    // sets MEINEXT.UPDATE to 1, updating MEICONTEXT with this context 
    csrrsi a0, 0xbe4, 0x2
    // if MSB set then no more active IRQs for this context
    bltz a0, no_more_irqs
dispatch_irq:
    // enable preemption by setting mstatus.mie
    csrsi mstatus, 0x8

    lui a1, %hi(__external_interrupt_table)
    add a1, a1, a0
    lw a1, %lo(__external_interrupt_table)(a1)
    jalr ra, a1

    // disable preemption while looking for new IRQ
    csrci mstatus, 0x8
    j get_next_irq

no_more_irqs:
    // restore meicontext, mstatus, mepc
    lw a2, 72(sp)
    lw a1, 68(sp)
    lw a0, 64(sp)

    csrw 0xbe5, a2 // MEICONTEXT
    csrw mstatus, a1
    csrw mepc, a0

    // restore caller-saved
    lw t6, 60(sp)
    lw t5, 56(sp)
    lw t4, 52(sp)
    lw t3, 48(sp)
    lw t2, 44(sp)
    lw t1, 40(sp)
    lw t0, 36(sp)
    lw a7, 32(sp)
    lw a6, 28(sp)
    lw a5, 24(sp)
    lw a4, 20(sp)
    lw a3, 16(sp)
    lw a2, 12(sp)
    lw a1, 8(sp)
    lw a0, 4(sp)
    lw ra, 0(sp)

    addi sp, sp, 76
    mret
```

Let's write a tests that checks two aspects of this: 1) lower priority
IRQs don't preempt ISRs and 2) higher priority IRQs DO preempt ISRs.
We can write a C source file that overrides the weak definitions of our isrs
to do this. Here's the [hideous (but functional) test I created](user/test_mei/main.c).

Ok, on the surface everything appears to be in working order.

#### Other Considerations

As I developed and tested these interrupt handlers, I found that some state
such as clock values and pending interrupts persisted between runs. Let's 
make sure that gets zeroed out on startup just like machine external interrupts.

Here's code that resets machine external interrupts, machine software
interrupts, and timer interrupts:

```
    // clear all IRQ force array bits
    // 4 iters * 16 bits = 64 bits cleared.
    // NOTE: MEIFA offset = 0xbe2
    li a0, 4
clear_meip:
    csrw 0xbe2u, a0 
    addi a0, a0, -1
    bgtz a0, clear_meip

    // clear software interrupts in core0 and core1, if pending
clear_msip:
    // SIO_BASE                 = 0xd0000000
    // RISCV_SOFTIRQ Offset     = 0x1a0
    li a1, 0xd00001a0 
    li a2, 0x300u
    sw a2, 0(a1)

    // clear timer interrupts and reset clocks
clear_mtip:
    // disable timer
    li a0, 0xd00001a4 // MTIME_CTRL
    sw zero, (a0)
    // set mtime to zero
    li a0, 0xd00001b0 // MTIME
    sw zero, (a0)
    sw zero, 4(a0)    // MTIMEH
```

Pretty straightforward stuff.

### Refactor Crap (12/22/24)

As of today, there are a bunch of little tests in the startup source code that
are making things a bit messy and unnecessarily annoying to navigate. There are
also a lot of hard coded addresses in the assembly code that are difficult to
read. Let's just fix that stuff and make a dedicated way to test our kernel
code.

Firstly, let's create a file for defining memory mapped addresses for the
rp2350, `rp2350.h`. I'll add register addresses on an as-needed basis
during development. While refactoring the contents of `startup.S`, I defined
the following:

```
#define RVCSR_MEIEA         0xbe0
#define RVCSR_MEIFA         0xbe2
#define RVCSR_MEINEXT       0xbe4
#define RVCSR_MEICONTEXT    0xbe5

#define BOOTRAM_BASE        0x400e0000

#define SIO_BASE            0xd0000000
#define SIO_RISCV_SOFTIRQ   0xd00001a0
#define SIO_MTIME_CTRL      0xd00001a4
#define SIO_MTIME           0xd00001b0
#define SIO_MTIMEH          0xd00001b4
#define SIO_MTIMECMP        0xd00001b8
#define SIO_MTIMECMPH       0xd00001bc
```

Then I included the header file in `startup.S` and replaced the immediates with
these definitions.

Now, to fix the testing. I currently have defined two kinds of tests. The first
kind are sanity checks defined in `startup.S`, written in assembly. They are
just labeled blocks of assembly code, and to execute the test requires only that
an instruction to jump to them after initialization be un-commented. This is
hacky, since the test code is being compiled into kernel, and running the tests
requires modifying the source code.

I also put a few tests in `user/` just so that I could run them from the
command line with `make run APP=<test-name>`. But this is also confusing,
because they don't actually execute in user mode.

I'll refactor the project to meet the following conditions:

1. Test code does not needlessly get compiled into the target binary.
2. Tests can be run from the command line like so:
```
make run TEST=<test-name>
```

Let's also put our tests in a `test/` folder.
In the makefile, we'll need to determine whether we're running an APP or a
TEST. We should also print an error if for some reason both are defined.

```
APP ?= $(if $(TEST),,blinky)
TARGET := $(if $(TEST),build/$(TEST).elf,build/$(APP).elf)
```

Instead of having `USER` variables, let's use generic `PROGRAM` variables to
derive our directory, sources, and objects:

```
PROGRAM_DIR := $(if $(TEST),test/$(TEST),user/$(APP))
PROGRAM_SRCS := $(wildcard $(PROGRAM_DIR)/*.c $(PROGRAM_DIR)/*.S)
PROGRAM_OBJ := $(BUILD_DIR)/$(if $(TEST),test_$(TEST),user_$(APP)).o
```

Finally, let's make our `run` command a bit more helpful:

```
run: $(GDB_TEMPLATE)
	@if [ -n "$(TEST)" ] && [ -n "$(APP)" ]; then \
		echo "Error: Cannot specify both APP and TEST"; \
		exit 1; \
	fi
	@echo "Running $(if $(TEST),test $(TEST),application $(APP))..."
	make compile
	sed "s|<PROGRAM>|$(TARGET)|" $(GDB_TEMPLATE) > init.gdb
	$(GDB) $(TARGET) -x init.gdb
```

On the source code refactor side of things, we can move both our assembly 
sanity checks and `user/` tests to `test/`. They should each get a nested
directory that will be used for all their sources and includes. They will be
linked and executed from their exposed `main` symbol, although we need not
obey the calling convention when refactoring our assembly tests. For example,
here is our new test for machine software interrupts, in
`test/test_mti/main.S`:

```
/**
 * @file main.S
 * @brief Sanity check for mti handler implementation.
 *
 * Doesn't obey calling conventions.
 *
 * @author Herbie Rand
 */

#define SIO_RISCV_SOFTIRQ   0xd00001a0
#define SIO_MTIME_CTRL      0xd00001a4
#define SIO_MTIME           0xd00001b0
#define SIO_MTIMEH          0xd00001b4
#define SIO_MTIMECMP        0xd00001b8
#define SIO_MTIMECMPH       0xd00001bc

.section .text
/**
 * @brief Tests timer interrupts (cause 7).
 *
 * If this executes correctly, we should reach the breakpoint in the weak
 * definition for `isr_mtimer_irq`.
 */
.global main
main:
    // enable mie.mtie
    li a0, 0x80
    csrs mie, a0

    // set mtime lower and higher half to 0
    li a0, SIO_MTIME
    sw zero, 0(a0) // MTIME
    sw zero, 4(a0) // MTIMEH
    
    // 1. write all ones to MTIMECMP
    li a0, SIO_MTIMECMP
    li a1, 0xffffffff // reset value
    sw a1, (a0)

    // 2. write value to MTIMECMPH
    li a0, SIO_MTIMECMPH
    sw zero, (a0)

    // 3. write value to MTIMECMP
    li a0, SIO_MTIMECMP
    li a1, 1
    sw a1, (a0)

    // enable the timer with MTIME_CTRL, set fullspeed with bit 1
    li a0, SIO_MTIME_CTRL
    li a1, 0x3
    sw a1, 0(a0)

    fence.i
    j _jail
```

I made a few other small changes, including changing `startup.S`'s definition
of `jail` to a `.global _jail` that can be invoked from these tests.
With these changes, it's now easy to run tests or programs by the name of their
directory in either `user/` or `test/`.

### User Mode (12/22/24 - 12/23/24)

Until now, we've been executing all of our code in Machine mode, which outside
of RISC-V parlance might be called "privileged" or "kernel" mode. When the
system is in machine mode, we get access to special registers, instructions,
and locations in memory. This is fine for our kernel code, but for applications
we may not trust (possibly errant applications), we should execute them only
with the required privileges and no more, so as to mitigate the risk of
corrupting or crashing the system. 

I've already been separating source files into `kernel/` and `user/` directories,
so that it is clear which code is intended to execute in user mode. Let us now 
turn to the RISC-V spec and privileged spec to learn how we may switch to user
mode.


> NOTE: we'll no longer be able to use the raspberry pi pico c sdk as a
> reference, as it does not provide facilities to switch between privilege
> modes!

From 8.6.4 Trap Return:

_"The MRET instruction is used to return from a trap taken into M-mode. MRET
first determines what the new privilege mode will be according to the values of
MPP and MPV in mstatus or mstatush, as encoded in Table 8.8. MRET then in
mstatus/mstatush sets MPV=0, MPP=0, MIE=MPIE, and MPIE=1. Lastly, MRET sets the
privilege mode as previously determined, and sets pc=mepc."_ [1]

The trap return sequence in 3.8.4 of the rp2350 datasheet is a useful
reference, and says this:

_"Hand-manipulating the trap handling CSRs is useful for low-level OS
operations such as context switching, or to make exception handlers return to
the instruction after the trap point by incrementing MEPC before return. You
can execute an mret without any prior trap, for example when entering U-mode
code from M-mode for the first time."_ [3]

So to transition to user mode, we should prepare the appropriate fields in
`mstatus` and then execute `mret`.

To set MPP to U-mode:

```
    li t0, 0xc00
    csrc mstatus, t0
```

And to set `mepc` to the start of our user program (`main`):

```
    la t0, main
    csrw mepc, t0
```

What about `MPIE`? According to the RISC-V privileged specification, higher
level privileged modes' interrupts should always be enabled (regardless of the
global interrupt enable bits in status registers) because otherwise there would
be no way for the higher privilege level to regain control over the system.
However, this subtlety was not implemented by Hazard3, as is explained in the
errata in the rp2350 datasheet (RP2350 E7). Thus we must manually set mpie so
that `mie` is set when we transition to U-mode with `mret`:

```
    li t0, 0x80
    csrs mstatus, t0
```

Another thing we must consider is which stack(s) we should use. Unlike ARM,
the RISC-V ISA doesn't offer much opinion on the handling of machine and user
stacks. Nonetheless, I believe we should have separate stack spaces for machine
and user mode so that down the line we may implement memory protection. Down
the line we may have many separate tasks, each with their own machine and user
stacks. For now, I'll define separate machine and user stacks in the linker
script:

```
    __mstack0_size = 0x2000;
    .mstack0 (NOLOAD) : ALIGN(4) {
        __mstack0_limit = .;
        . += __mstack0_size;
        __mstack0_base = .;
    } > RAM


    __mstack1_size = 0x2000;
    .mstack1 (NOLOAD) : ALIGN(4) {
        __mstack1_limit = .;
        . += __mstack1_size;
        __mstack1_base = .;
    } > RAM

    __ustack0_size = 0x2000;
    .ustack0 (NOLOAD) : ALIGN(4) {
        __ustack0_limit = .;
        . += __ustack0_size;
        __ustack0_base = .;
    } > RAM


    __ustack1_size = 0x2000;
    .ustack1 (NOLOAD) : ALIGN(4) {
        __ustack1_limit = .;
        . += __ustack1_size;
        __ustack1_base = .;
    } > RAM
```

Then we can set the new stack pointer before we execute `mret`: 

```
    la sp, __ustack0_base
```

This all appears to work fine, but I have already defined several tests that
must execute in machine mode to force particular interrupts. And we may want to
implement more machine mode tests in the future. So as to support these kinds
of tests, we can use a c preprocessor directive to conditionally jump to main
in machine mode if we're running a test. The test can then enter user mode if
it so chooses. In `kernel/startup.S`:

```
#ifdef IS_TEST
    jal main
#else
enter_user_mode:
    // instructions leading up to mret...
#endif
```

In our Makefile, we must provide the `IS_TEST` CFLAG, plus we must compile
our kernel separately for tests and user applications (since the C source will
be different). To address the first:

```
CFLAGS = $(ARCHFLAGS) -g -nostdlib -nodefaultlibs $(if $(TEST),-DIS_TEST,)
```

...and the second:

```
KERNEL_OBJ := $(BUILD_DIR)/kernel$(if $(TEST),_test,).o
```

### Interrupt Driven Blinky (12/23/24 - ?)

Let's now use our interrupt handling implementations, plus user mode, to 
rewrite our blinky program. I'll do this for several purposes:

1. To gain familiarity with the RISC-V platform timer (`mtime`).
2. To test our interrupt handling implementation.
3. To ensure that our user mode code plays nice with the switches to machine
mode during interrupt handling.
4. To build out a way for our user level code to invoke system calls.
5. To build out timing facilities/libraries that we can use in the future.
6. To build out a gpio module that we can use and build on in the future.

Let's start by creating a simple GPIO library. Then, let's create a system
call to turn on and off the on-board LED.

From section _section_, we can see that the clock can be configured to use
multiple sources. 

From the RISC-V spec, we can learn how to use `mtime` to produce interrupts.

...

Running the program, we can discern the clock source and frequency.

`CLK_SYS_CTRL` Register:

```
(gdb) p/x *(unsigned long *)0x4001003c 
$1 = 0x0
```

This indicates that `CLK_REF` is being used. We can check the selected ref
source with the `CLK_REF_CTRL` register.
```
(gdb) p/x *(unsigned long *)0x40010038
$3 = 0x1
```

This "one-hot" encoding indicates source 0x0 in the `CLK_REF_CTRL` register,
corroborated by reading that register directly:
```
(gdb) p/x *(unsigned long *)0x40010030
$4 = 0x0
```

In the datasheet, this indicates that the ROSC_CLKSRC_PH is being used for the
reference clock, and to my understanding the clock driving the `mtime` counter.

From 8.1.1.2, it appears that the Ring Oscillator (ROSC) runs nominally at 11
MHz, but may vary quite a bit due to temperature and environment. When we need
greater precision and consistency, we should consider other clocks to drive our
counter. However, for rewriting blinky, it is suitable. 

With this, we can write a simple driver to enable and start the system timer using
`mtime` and `mtimecmp`, as well as `mtimecmph`. We must take special care to avoid
overflow when determining the top counter value, and to make use of the upper 32
bits for the countertop in `mtimecmph`. Here is a simple implementation of `mtime.c`:

```
#include "mtime.h"
#include "rp2350.h"
#include "types.h"

/** @brief ROSC nominal frequency is 11 MHz */
#define ROSC_NOMINAL_MHZ 11

void mtimer_enable() {
    asm volatile("li a0, 0x80\n\t"
                 "csrs mie, a0\n\t"
                 :
                 :
                 : "a0");
}

// NOTE: assumes ROSC
int mtimer_start(uint32_t us) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint32_t coef = ROSC_NOMINAL_MHZ;

    // safe 32-bit * 32-bit multiplication
    // note that exceeding 64 bits is impossible
    while (coef--) {
        if (lo + us < lo) {
            hi++;
        }
        lo += us;
    }

    *(uint32_t *)SIO_MTIME_CTRL = 0;
    *(uint32_t *)SIO_MTIME = 0;
    *(uint32_t *)SIO_MTIMEH = 0;
    *(uint32_t *)SIO_MTIMECMP = (uint32_t)-1;
    *(uint32_t *)SIO_MTIMECMPH = hi;
    *(uint32_t *)SIO_MTIMECMP = lo;
    *(uint32_t *)SIO_MTIME_CTRL = 3;
    return 0;
}
```

Since we'll likely reset the system timer to its previous value, it seems natural
to avoid re-computing `mtimecmp` and `mtimecmph`. Let's implement a simple
size-1 cache for this case. Here is `mtime.h`:


```
#ifndef MTIME_H
#define MTIME_H

#include "rp2350.h"
#include "types.h"

/**
 * @brief Cache structure to prevent re-computing mtimecmph.
 *
 * May be helpful in case mtimecmph computations are expensive,
 * and/or the mtime counter is frequently reset.
 */
typedef struct {
    /** @brief Milliseconds (cache key) */
    uint32_t us;
    /** @brief Cached mtimecmp value */
    uint32_t mtimecmp;
    /** @brief Cached mtimecmph value */
    uint32_t mtimecmph;
} mtime_cache_t;

/**
 * @brief Enables the mtime timer interrupt.
 * You can implement the interrupt handler by overriding
 * the weak definition for `void isr_mtimer_irq()`.
 */
void mtimer_enable();

/**
 * @brief Starts the RISC-V mtime timer, interrupting at the provided duration.
 * @param us    Integer microseconds indicating duration before interrupt.
 * @return 0 on success, nonzero on error
 */
int mtimer_start(uint32_t us);

/**
 * @brief Stops the mtime timer from ticking.
 */

#endif
```

...and here is `mtime.c`:

```
#include "mtime.h"
#include "rp2350.h"
#include "types.h"

/** @brief ROSC nominal frequency is 11 MHz */
#define ROSC_NOMINAL_MHZ 11

static mtime_cache_t cache;

void mtimer_enable() {
    asm volatile("li a0, 0x80\n\t"
                 "csrs mie, a0\n\t"
                 :
                 :
                 : "a0");
}

// NOTE: assumes ROSC
int mtimer_start(uint32_t us) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint32_t coef = ROSC_NOMINAL_MHZ;

    if (us == cache.us) {
        lo = cache.mtimecmp;
        hi = cache.mtimecmph;
    } else {
        // safe 32-bit * 32-bit multiplication
        // note that exceeding 64 bits is impossible
        while (coef--) {
            if (lo + us < lo) {
                hi++;
            }
            lo += us;
        }
        cache.us = us;
        cache.mtimecmp = lo;
        cache.mtimecmph = hi;
    }

    *(uint32_t *)SIO_MTIME_CTRL = 0;
    *(uint32_t *)SIO_MTIME = 0;
    *(uint32_t *)SIO_MTIMEH = 0;
    *(uint32_t *)SIO_MTIMECMP = (uint32_t)-1;
    *(uint32_t *)SIO_MTIMECMPH = hi;
    *(uint32_t *)SIO_MTIMECMP = lo;
    *(uint32_t *)SIO_MTIME_CTRL = 3;
    return 0;
}
```

And to test our implementation, `test/test_led/main.c`:

```
/**
 * @brief Tests blinky with mtimer interrupt.
 *
 * The actual rate of blinking depends on the clock source and environment.
 * However, assuming that the clock uses the default ROSC at nominal clock
 * rate of 11 MHz, `mtimer_start` will behave correctly, and the led will
 * blink on for 0.5 seconds, off for 0.5 seconds, repeatedly.
 *
 * @author Herbie Rand
 */
#include "gpio.h"
#include "mtime.h"
#include "riscv.h"
#include "types.h"

#define LED_PIN 25

static uint8_t on = 0;
static uint32_t us = 500000;

int main() {
    mtimer_enable();
    gpio_init(LED_PIN);
    if (mtimer_start(us)) {
        asm volatile("ebreak");
    }
    while (1) {
        asm volatile("wfi");
    }
    return 0;
}

void isr_mtimer_irq() {
    if (!on) {
        gpio_set(LED_PIN);
    } else {
        gpio_clr(LED_PIN);
    }
    on = ~on;
    mtimer_start(us);
}
```

It's great that we now have interrupt-driven blinky, but it's not yet working as
a user application. Generally speaking, we need a way for our user applications
to safely and securely request the kernel perform operations (e.g. operations on
privileged segments, CSRs) on the application's behalf, aka we need to
implement system calls. We don't have things like memory protection yet, but
we'll probably want that stuff in the future.

### ... CUT to 1/01/2025

Ok, so it turns out I am an idiot -- user mode was not properly being entered 
nor executed. Firstly, this is the actual correct way to set `MPP` to enter
user mode (`kernel/startup.S`:

```
    // set mstatus MPP to U-mode
    li t0, 0x1800
    csrc mstatus, t0
```

The next problem we have is that the user mode, by default, does not have
permissions to execute its own text region. So we should modify the linker
script to separate the kernel and application text regions, and update the
system privileges at boot time to let the user text region be executed. 

First, separating out machine mode and user mode text regions (similar to 
how we separated the user and machine stacks):

```
        __text_start = .;
        __mtext_start = .;
        kernel*.o(.text*)
        . = ALIGN(4);
        __mtext_end = .;
        __utext_start = .;
        user*.o(.text*)
        . = ALIGN(4);
        __utext_end = .;
        __text_end = .;
```

I'd like to eventually make this a bit more robust so that any arbitrary
binary's text section can be inserted here instead of relying on file names.
We'll do that when we improve the Makefile again to individually compile kernel and user modules.

While doing this, I also split up (and created symbols for) machine and user
mode data, rodata, and bss segments.

Now, let us apply the correct permissions to our user text segment.

...stuff...

I've been making various changes as I learn and implement this stuff. I've made
further changes to the linker script, and have noticed that there are notable
hardware bugs which will also constrain our implementation:

1. RP2350-E3
2. RP2350-E6

But (1) gives us a great excuse to start implementing system calls. From the
description:

_"This means that granting Non-secure access to the PADS registers in the QFN-60
package does not allow Non-secure software to control the correct pads. It may
also allow Non-secure control of pads that are not granted in GPIO_NSMASK0."_

And the suggested workaround:

_"Disable Non-secure access to the PADS registers by clearing PADS_BANK0.NSP,
NSU. Implement a Secure Gateway (Arm) or ecall handler (RISC-V) to permit
Non-secure/U-mode code to read/write its assigned PADS_BANK0 registers."_

Essentially, our `ecall` handler will be our system call dispatcher. User mode
code will execute an environment call, which will trap us into machine mode,
where the kernel service will be invoked.

If we add an `ecall` to the beginning of our blinky program, we will see that
we enter `isr_env_umode_exc`, as expected. And printing `mcause` gives `0x8`,
indicating that the cause of the exception is a U-mode environment call, as
expected. Since we'll want to implement multiple system calls, we need to find
a way to distinguish the kind of service requested. There isn't too much detail
about how to pass parameters in the datasheet or RISC-V spec, but the
unprivileged spec says this:

_"The ECALL instruction is used to make a service request to the execution
environment. The EEI will define how parameters for the service request are
passed, but usually these will be in defined locations in the integer register
file."_ [2]

This gives us some flexibility. We must decide for ourselves how kernel services
will be requested and how to pass arguments.

At the time that `isr_env_umode_exc` is invoked, we know that our generic
exception handling code will have pushed caller saved registers to the stack.
With the knowledge that these were just pushed and the current stack pointer,
we can set arguments in our user-code as normal and recover them in our U-mode
`ecall` handler. Let's implement a system call to turn on the on-board LED. 

### ...CUT -- see commit fa54edf7ecbb17d671a6148b40601d2bbf12bade (01/07/2024)

Ok, so after making some fixes related to testing, the linker script, Makefile,
and project structure, I now have separate `user/` and `apps/` directories.
`user/` will contain user libraries including system call stubs, whereas `apps/`
will contain instances of programs, where a single app is built into the RTOS,
and it may use any `user/` library code. `include/` contains definitions
accessible to all code, including kernel code. With that, let's create a blinky
application that uses a system call to set the LED at GPIO pin 25.

In `user/`, we'll define the libraries that will wrap system calls.
In `user/led.h`:

```
#ifndef LED_H
#define LED_H

/**
 * @brief Turns on the on-board LED.
 */
void led_on();

/**
 * @brief Turns off the on-board LED.
 */
void led_off();

#endif
```

Let's distinguish the system calls using macros in `include/sys.h`, accessible 
to both the user code and the kernel:

```
#ifndef SYS_H
#define SYS_H

#define SYS_LED_ON  0
#define SYS_LED_OFF 1

#endif
```

Now let's invoke the system call with `ecall` in `user/usys.S`:
```
#include led.h
#include sys.h

.global led_on
led_on:
    li a7, SYS_LED_ON
    ecall
    ret

.global led_off
led_off:
    li a7, SYS_LED_OFF
    ecall
    ret
```

By convention, we'll use `a7` to store the number for the system call. We do
this because it enables us to define prototypes with more args (0-6), and we
won't need to modify the other argument registers so long as we reserve `a7`
for passing the system call number.

Now, let's implement system call dispatching in the kernel. Let's start by
overriding the weak definition of `isr_env_umode_exc` in a new files
`kernel/syscall.h` and `kernel/syscall.c`, and creating system calls for
turning the LED on and off.

`kernel/syscall.h`:
```
/**
 * @brief Contains syscall prototypes.
 */
#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

/** @brief Exception frame, pushed onto the stack during ecall */
typedef struct {
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
} exception_frame_t;

/**
 * @brief Overrides weak umode ecall service routine.
 * @param exception_frame_t containing syscall args
 */
void isr_env_umode_exc(exception_frame_t *);

/**
 * @brief Uses GPIO to turn on the LED.
 * @param exception_frame_t containing syscall args
 */
void sys_led_on(exception_frame_t *);

/**
 * @brief Uses GPIO to turn off the LED.
 * @param exception_frame_t containing syscall args
 */
void sys_led_off(exception_frame_t *);

#endif
```

`kernel/syscall.c`:

```c
#include "asm.h"
#include "gpio.h"
#include "rp2350.h"
#include "sys.h"
#include "syscall.h"
#include "types.h"

#define LED_PIN 25

static uint8_t led_init = 0;

static void (*syscall_table[])(exception_frame_t *) = {
    [SYS_LED_ON] sys_led_on,
    [SYS_LED_OFF] sys_led_off,
};

void isr_env_umode_exc(exception_frame_t *sf) {
    if (sf->a7 >= SYSCALL_COUNT) {
        // should never reach here
        breakpoint();
    }
    syscall_table[sf->a7](sf);
    inc_mepc();
}

void sys_led_on(exception_frame_t *sf) {
    (void)sf;
    if (!led_init) {
        gpio_init(LED_PIN);
        led_init = 1;
    }
    gpio_set(LED_PIN);
}

void sys_led_off(exception_frame_t *sf) {
    (void)sf;
    gpio_clr(LED_PIN);
}
```

As you may have noticed, I've also defined some helpers in `asm.h` and `asm.c`.
See the source for details.

Now, let's implement the blinky app, this time as a proper user mode
application that invokes system calls. `apps/blinky/main.c`:

```c
#include "led.h"
#include "time.h"

int main() {
    while (1) {
        led_on();
        for (int i = 0; i < 5000000; i++);
        led_off();
        for (int i = 0; i < 5000000; i++);
    }

    return 0;
}
```

One more thing you need to do is make sure that user code can read/write to the
user stack, plus fetch and execute user code. After defining some linker
symbols, I accomplished this in `kernel/startup.S`, paying special attention
to the rp2350 documentation and taking note of the errata about incorrect RWX
bit order in the PMP configuration registers [3]:

```
    // set user text execute permissions
    la t0, __utext_start
    srli t0, t0, 2
    li t1, 0x3ff // 4 KB, equal to user text size
    or t0, t0, t1
    csrw RVCSR_PMPADDR0, t0

    // set user stack read/write permissions
    la t0, __ustack0_limit
    srli t0, t0, 2
    li t1, 0x3ff // 4 KB, equal to user stack sizes
    or t0, t0, t1
    csrw RVCSR_PMPADDR1, t0

    // NOTE: Per RP2350-E6, R-W-X is the order to PMPCFG
    // set address mode to NAPOT and X perms
    // CFG 0 --> 0001 1001 --> 0x19 --> NAPOT, X  perms, NOTE E6
    // CFG 1 --> 0001 1110 --> 0x1E --> NAPOT, RW perms, NOTE E6
    li t0, 0x1e19
    csrw RVCSR_PMPCFG0, t0
```

The result is a working blinky app.

### Return of the Makefile (01/08/2024)

I've come to learn that rewriting the Makefile is going to be a routine part of
development. Although, hopefully we're getting to a pretty stable state. I want
to make two improvements:

1. Compile all modules (objects) individually.
2. Use a linker script template so we can specify the exact binaries used
   before link time.

Let's start by compiling all objects individually. Instead of compiling all
sources into `kernel.o`, `user.o`, etc, let's create build subdirectories for
each kind of binary before linking. The build directory will mirror our project
structure, so something like this:

```
build/kernel/
build/user/
build/apps/...
build/test/...
```

To compile modules individually, let's redefine our Makefile variables to
determine our sources and objects. For example, for our user libraries:

```
USER_DIR := user
USER_C_SRCS := $(wildcard $(USER_DIR)/*.c)
USER_ASM_SRCS := $(wildcard $(USER_DIR)/*.S)
USER_BUILD_DIR := $(BUILD_DIR)/$(USER_DIR)
USER_OBJS := $(USER_C_SRCS:$(USER_DIR)/%.c=$(USER_BUILD_DIR)/%.o) \
			 $(USER_ASM_SRCS:$(USER_DIR)/%.S=$(USER_BUILD_DIR)/%.o) 
```

And the corresponding rules will compile each object:

```
$(USER_BUILD_DIR)/%.o: $(USER_DIR)/%.S
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(CFLAGS) -I $(USER_DIR) -c $< -o $@

$(USER_BUILD_DIR)/%.o: $(USER_DIR)/%.c
	@mkdir -p $(USER_BUILD_DIR)
	$(CC) $(CFLAGS) -I $(USER_DIR) -c $< -o $@
```

Likewise for compiling kernel and program files.
On the linker script side of things, I defined a linker script template instead,
which uses sed to substitute some patterns before creating the linker script
used to create our final elf executable. In our linker template, there are
lines like this:

```
<KERNEL_BUILD_DIR>/*.o(.text)
```

Then, in the rule where we create our elf file, before the linking step:
```
	@sed -e "s|<KERNEL_BUILD_DIR>|$(KERNEL_BUILD_DIR)|g" \
		 -e "s|<USER_BUILD_DIR>|$(USER_BUILD_DIR)|g" \
		 -e "s|<PROGRAM_BUILD_DIR>|$(PROGRAM_BUILD_DIR)|g" \
		$(MEMMAP_TEMPLATE) > $(MEMMAP)
```

This should make it possible for us to add, remove, and rename source files as
needed with minimal modifications to the linker script. Full details in the
commit with hash 4751f09cb8e93a5b465a71244c50bda35d2204a7.

### Core 1 Initialization (12/23/24 - 02/03/2025)

So far we've just been sending core 1 to jail if it executes on startup.
However, we want to be able to use our second core. To understand how we can
make use of core 1, I attempt to answer my own questions:

1. What is the core 1 startup sequence?
2. What configuration is shared between cores?
3. How do interrupts work?
4. How do the cores communicate with one another?
5. What should core 1 do after booting?

---

#### 1. What is the core 1 startup sequence? 

The core 1 startup sequence is in the datasheet [3]. We can write a helper for
this sequence.

#### 2. What configuration is shared between cores?

The cores share memory (the address space) and most registers. Found exceptions:

- ALL CSRS!!!
- PERI_NONSEC   (SIO_BASE + 0x190)
- NMI_MASK0     (EPPB_BASE + 0x0)
- NMI_MASK1     (EPPB_BASE + 0x4)
- SLEEPCTRL     (EPPB_BASE + 0x8) <-- ?

#### 3. How do interrupts work?

Some interrupts are core local:

- SIO_IRQ_FIFO
- SIO_IRQ_FIFO_NS
- SIO_IRQ_MTIMECMP
- IO_IRQ_BANK0
- IRQ_IO_BANK0_NS
- IO_IRQ_QSPI
- IO_IRQ_QSPI_NS

All other interrupts should be mirrored on both cores. Section 3.2 of the
datasheet says this:

_"Non core-local interrupts should only be enabled in the interrupt controller
of a single core at a time, and will be serviced by the core whose interrupt
controller they are enabled in."_ [3]

#### 4. How do the cores communicate with one another?

Resources for inter-core communication include hardware spinlocks,
inter-processor FIFOs, and doorbells.

#### 5. What should core 1 do after booting?

It depends. Core 1 may be manually configured to only execute certain tasks.
However, I'd like to initialize a more general runtime that does symmetric
multiprocessing.

---

### Core 1 Initialization Continued (02/02/2025 - 02/03/2025)

Let's start by getting some simple task to execute on core 1. Core 0 will
initialize core 1 and then spin indefinitely. Core 1 will execute blinky.

We'll implement this as a test with machine mode privileges. Then, after
seeing what core 1 initialization looks like, we can add basic configuration
to our kernel. 

The rp2350 datasheet provides a not-so-pseudo code sequence for initializing
core 1, which by default just loops in bootrom: 

```c
// values to be sent in order over the FIFO from core 0 to core 1
//
// vector_table is value for VTOR register
// sp is initial stack pointer (SP)
// entry is the initial program counter (PC) (don't forget to set the thumb bit!)
const uint32_t cmd_sequence[] =
{0, 0, 1, (uintptr_t) vector_table, (uintptr_t) sp, (uintptr_t) entry};

uint seq = 0;

do {
    uint cmd = cmd_sequence[seq];
    // always drain the READ FIFO (from core 1) before sending a 0
    if (!cmd) {
        // discard data from read FIFO until empty
        multicore_fifo_drain();
        // execute a SEV as core 1 may be waiting for FIFO space
        __sev();
    }
    // write 32 bit value to write FIFO
    multicore_fifo_push_blocking(cmd);
    // read 32 bit value from read FIFO once available
    uint32_t response = multicore_fifo_pop_blocking();
    // move to next state on correct response (echo-d value) otherwise start over
    seq = cmd == response ? seq + 1 : 0;
} while (seq < count_of(cmd_sequence));
```

As you can see, core 1 is initialized with a pointer to the vector table, a stack
pointer, and an entry point. These are sent in a defined sequence over the
interprocessor FIFO queues.

We can use this as a starting point for our test. Let us define the following
functions (in order of appearance):

```c
void multicore_fifo_drain();
void __sev();
void multicore_fifo_push_blocking(uint32_t);
uint32_t multicore_fifo_pop_blocking();
```

We can gather from context that `multicore_fifo_drain()` should read all contents
from the FIFO, thus emptying the queue. It makes sense that the routine is 
`multicore` because the code is core independent, (references to the FIFO
registers are core-local). We can check the least significant bit in `FIFO_ST`
to determine if the FIFO is empty, and we can read a value by loading from the
`FIFO_RD` address:

```c
#define SIO_FIFO_ST       0xd0000050
#define SIO_FIFO_RD       0xd0000058

void multicore_fifo_drain() {
    uint32_t rd;
    // read until RX is empty
    while (*(uint32_t *)SIO_FIFO_ST & 0x1) {
        rd = *(uint32_t *)SIO_FIFO_RD;
    }
    (void)rd;
}
```

`__sev()` notifies the opposite core that an event has occurred. In arm, there
is a sev instruction, but for RISC-V and our interrupt controller, there is a
special instruction provided by our hazard3 extension to accomplish the same
thing, and the datasheet suggests a macro. Here is the resulting code:

```c
#define __h3_unblock() asm("slt x0, x0, x1")

void __sev() {
    __h3_unblock();
}
```

Next we must write the command to the interprocessor FIFO. We should check that
there is space in the FIFO before writing, and then notify the core via a `__sev()`:

```c
#define SIO_FIFO_WR       0xd0000054

void multicore_fifo_push_blocking(uint32_t cmd) {
    while (!(*(uint32_t *)SIO_FIFO_ST & 0x2))
        ;
    *(uint32_t *)SIO_FIFO_WR = cmd;
    __sev();
}
```

`multicore_fifo_pop_blocking` should simply wait until a message is available
in the FIFO, then return it:

```c
uint32_t multicore_fifo_pop_blocking() {
    while (!(*(uint32_t *)SIO_FIFO_ST & 0x1))
        ;
    return *(uint32_t *)SIO_FIFO_RD;
}
```

With this, we can put everything together in a function `init_core1()`. Let's
have the function accept arguments to the vector table, stack pointer, and
function pointer where it will begin executing.

```c
void init_core1(uint32_t, uint32_t, uint32_t);

void init_core1(uint32_t vt, uint32_t sp, uint32_t fn) {
    uint32_t cmd;
    uint32_t resp;
    uint32_t cmd_sequence[6];

    cmd_sequence[0] = 0;
    cmd_sequence[1] = 0;
    cmd_sequence[2] = 1;
    cmd_sequence[3] = vt;
    cmd_sequence[4] = sp;
    cmd_sequence[5] = fn;

    uint32_t seq = 0;
    do {
        cmd = cmd_sequence[seq];
        if (!cmd) {
            multicore_fifo_drain();
            __sev();
        }
        multicore_fifo_push_blocking(cmd);
        resp = multicore_fifo_pop_blocking();
        seq = (cmd == resp) ? (seq + 1) : 0;
    } while (seq < 6);
}
```

This is sufficient to begin executing code on core 1. However, to actually use
core 1 for our interrupt driven blinky program, we need to repeat some of the
initial configuration that core 0 gets in `kernel/startup.S`. Now seems to be a
good time to build up some helpers for modifying CSRs in c code. I won't go over
them here, but they are defined in `kernel/asm.h` and `kernel/asm.S`. 

To use the timer interrupts, we must enable interrupts globally in `mstatus` on
core 1. Recall that all RISC-V CSRs are core-local. It also seems to be a good idea
to clear pending interrupts in MEIFA. Finally, I modified the `mtimer_enable`
code to clear pending timer interrupts (assuming they are leftover for some
previous use of the timer, no longer needed).

After putting everything together, here is the resulting test
(`test/test_blinky_core1/main.c`):

```c
/**
 * @brief Tests core 1 initialization by running blinky with mtimer interrupt
 *        on core 1.
 *
 * Expected behavior is blinking LED, identical to test_blinky_interrupt.
 * However, core 0 should spin indefinitely, and core 1 should be executing
 * the mtimer interrupt handler.
 *
 * @author Herbie Rand
 */
#include "gpio.h"
#include "mtime.h"
#include "riscv.h"
#include "types.h"
#include "asm.h"
#include "rp2350.h"

#define LED_PIN 25

void init_core1(uint32_t, uint32_t, uint32_t);
void multicore_fifo_drain();
void __sev();
void multicore_fifo_push_blocking(uint32_t);
uint32_t multicore_fifo_pop_blocking();

void blinky();
void isr_mtimer_irq();

extern uint32_t __vector_table;
extern uint32_t __mstack1_base;

uint32_t *vt = &__vector_table;
uint32_t *sp1 = &__mstack1_base;

static uint8_t on = 0;
static uint32_t us = 5000000;

int main() {
    init_core1((uint32_t)vt + 1, (uint32_t)sp1, (uint32_t)blinky);
    while (1) {
        asm volatile("wfi");
    }
    return 0;
}

void init_core1(uint32_t vt, uint32_t sp, uint32_t fn) {
    uint32_t cmd;
    uint32_t resp;
    uint32_t cmd_sequence[6];

    cmd_sequence[0] = 0;
    cmd_sequence[1] = 0;
    cmd_sequence[2] = 1;
    cmd_sequence[3] = vt;
    cmd_sequence[4] = sp;
    cmd_sequence[5] = fn;

    uint32_t seq = 0;
    do {
        cmd = cmd_sequence[seq];
        if (!cmd) {
            multicore_fifo_drain();
            __sev();
        }
        multicore_fifo_push_blocking(cmd);
        resp = multicore_fifo_pop_blocking();
        seq = (cmd == resp) ? (seq + 1) : 0;
    } while (seq < 6);
}

void multicore_fifo_drain() {
    uint32_t rd;
    // read until RX is empty
    while (*(uint32_t *)SIO_FIFO_ST & 0x1) {
        rd = *(uint32_t *)SIO_FIFO_RD;
    }
    (void)rd;
}

void __sev() {
    __h3_unblock();
}

void multicore_fifo_push_blocking(uint32_t cmd) {
    while (!(*(uint32_t *)SIO_FIFO_ST & 0x2))
        ;
    *(uint32_t *)SIO_FIFO_WR = cmd;
    __sev();
}

uint32_t multicore_fifo_pop_blocking() {
    while (!(*(uint32_t *)SIO_FIFO_ST & 0x1))
        ;
    return *(uint32_t *)SIO_FIFO_RD;
}

void blinky() {
    // enable external interrupts on core 1
    set_mstatus(MIE_MASK);
    clr_meifa();
    mtimer_enable();
    gpio_init(LED_PIN);

    if (mtimer_start(us)) {
        asm volatile("ebreak");
    }
    while (1) {
        asm volatile("wfi");
    }
}

void isr_mtimer_irq() {
    if (!on) {
        gpio_set(LED_PIN);
    } else {
        gpio_clr(LED_PIN);
    }
    on = ~on;
    mtimer_start(us);
}
```

Note that it's important that core 0 does NOT have the timer interrupt enabled.
We want only core 1 controlling the LED in this test.

After getting things working, I put the useful core 1 initialization routines
in `kernel/runtime.{h,c}` and `kernel/fifo.{h,c}`, then refactored
`kernel/test_blinky_core1`.

### UART Console (02/03/2025 - ?)

Let us build a bi-directional UART console, a valuable utility for
communicating with and debugging our OS. I wired the pico 2 in accordance with
the debug probe documentation [5]. Notably, the wiring of GPIO pins used for
UART writes and reads creates a bidirectional connection:

```
Debug Probe            Pico 2
-----------            ------
UART_TX      <--->     GPIO 0 (UART_RX default)
UART_RX      <--->     GPIO 1 (UART_TX default)
```

The `minicom` program can be used on the development host to connect with the
debugger. What's left is to develop the UART driver and read/write utilities to
make meaningful communications with the debugger.

### Clocks Revisited (02/05/2025 - ?)

The half-baked mtimer module I created only goes so far, and to drive more precise
communication requires a better understanding of the clocks in the system. To this
end I created `kernel/clock.{h,c}` to get and set clock sources, measure clock
frequencies, and more.



# References

1. riscv-privileged-20211203.pdf
2. riscv-20191213.pdf
3. rp2350-datasheet.pdf
4. pico-2-pinout.pdf
5. debug probe documentation (https://www.raspberrypi.com/documentation/microcontrollers/debug-probe.html)
