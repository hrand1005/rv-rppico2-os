/**
 * This entry point will be located at the binary's the lowest address.
 * It is the first thing the bootrom will execute after interpreting the
 * metadata in the binary info header section.
 */
.section .reset, "ax"
.global _entry_point
_entry_point:
    j _reset_handler

_reset_handler:
    /* TODO: load global pointer */

    /* send core1 to jail if executing */
    csrr a0, mhartid
    bnez a0, jail

    /* initialize the stack pointer */
    la sp, __stack0_base

    /* mount the vector table */
    la a0, __vector_table
    ori a0, a0, 1
    csrw mtvec, a0

    /*
     * 1. initialize xip, load secondary bootloader from flash if necessary
     */

    /*
     * 2. clear all IRQ enable array bits
     * NOTE: apparently this is done automatically on reset
     * However, if you had to, you could add this to the following loop:
     *
     * csrw 0xbe0u, a0  /* RVCSR_MEIEA_OFFSET _u(0x00000be0)
     */

    /*
     * 3. clear all IRQ force array bits
     * 32 iters * 16 bits = 512 bits cleared.
     * NOTE: MEIFA offset = 0xbe2
     */
    li a0, 32
clear_meip:
    csrw 0xbe2u, a0 
    addi a0, a0, -1
    bgtz a0, clear_meip
    
    /* 4. enable external interrupts */

    /*
     * 4a. enable mie.meie
     * NOTE: 0x800u = cause 11 (MEIE)
     * NOTE: MIE at offset 0x304, so that could work too, apparently
     */
    li a0, 0x800u
    csrw mie, a0

    /*
     * 4b. enable mstatus.mie
     * NOTE: MSTATUS at offset 0x300, so that could work too, apparently
     * NOTE: 0x8u = Bit 3 (represents mie in mstatus)
     */
    csrsi mstatus, 0x8u

    /*
     * 5. clear mscratch, which detects nested machine exceptions
     */
    csrw mscratch, zero

    /* test machine enabled interrupts */
    j test_mei_irq0

    ebreak

jail:
    ebreak
    j jail


/**
 * Tests external interrupts (cause 11, IRQ 1)
 */
test_mei_irq0:
    /* First, enable the corresponding mei IRQ number, let's say IRQ 1
     * 
     * MEIEA Offset: 0x00000be0u
     * Bit key for MEIEA: W=Window, R=Reserved, I=Index
     * 
     * WWWW WWWW WWWW WWWW RRRR RRRR RRRI IIII
     *
     * So, to set IRQ 1, we need to set the rightmost W to 1.
     * 
     * 0000 0000 0000 0001 0000 0000 0000 0000
     *
     * Which equals 0x00010000u, so let us write this content to MEIEA.
     * This will _enable_ interrupt request number 1, which is a machine
     * external interrupt.
     */
    li a0, 0x10000u
    csrw 0xbe0u, a0

    /* Next, we can create the interrupt request using MEIFA.
     *
     * MEIFA Offset: 0x00000be2u
     * 
     * The register works the same as MEIA, except this time writing the
     * literal will force an interrupt.
     */
    csrw 0xbe2u, a0

    fence.i

    /* We shouldn't make it this far, so go to jail */
    j jail

/**
 * Note that in X mode, the vector table must be 64 byte aligned.
 * See rp2350 datasheet section 3.8.4.2.1
 * See riscv-privileged-20211203 section 3.1.7
 */
.section .vectors, "ax"
.p2align 6
__vector_table:

/** disable compressed instruction encodings */
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

/**
 * This is the initial IMAGE_DEF item.
 * See rp2350 datasheet section 5.9.3.1 (IMAGE_DEF)
 * See rp2350 datasheet section 5.9.5.2 (Minimum Viable Image Definition)
 *
 * There is additional startup configuration that could be encoded here.
 * Instead, with this minimal configuration, the bootrom simply starts
 * executing at the lowest address (0x10000000), where we have defined
 * our entry point.
 *
 * Note that this must be found in the first 4 KB of flash.
 */
.section .image_def, "a"
.word 0xffffded3
.word 0x11010142
.word 0x000001ff
.word 0x00000000
.word 0xab123579

