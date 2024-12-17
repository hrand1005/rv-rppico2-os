# RISC-V RTOS for RP2350 Log

Logging the development of an operating system for the raspberry pi pico 2.
The OS will be compiled for the RISC-V architecture (because I want to) and will
thus run on the hazard3 cores in the rp2350 processor.

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

### Exception & Interrupt Handling

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

#### External Interrupt Hanlding

We must also handle timer interrupts (`isr_mei`). Here's what the RISC-V
privileged ISA has to say about external interrupts:

The Xh3irq is an "extension" that is _"architected as a layer on top of the
standard `mip.meip` external interrupt line"_ [3]. Recall from the RISC-V spec
that `mip.meip` represents the machine external interrupt pending vector. thus
what follows relates to the handling of "machine external interrupts", and does
not necessarily affect timer and software interrupts.

# References

1. riscv-privileged-20211203.pdf
2. riscv-20191213.pdf
3. rp2350-datasheet.pdf
4. pico-2-pinout.pdf
