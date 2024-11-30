# Bare Metal RISC-V

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

The slew of acronyms in the privileged spec and rp2350 datasheet are a bit
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

At this point, we have enabled machine interrupts `mstatus.mie`, and specifically
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

If we peek at the pico-sdk bootstrap code, we can see that they also clear
`mscratch` here. Seems like good practice, so we may as well include it in
our implementation:

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

### Interrupt Handling


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

# References

1. riscv-privileged-20211203.pdf
2. riscv-20191213.pdf
3. rp2350-datasheet.pdf
