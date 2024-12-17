/**
 * @file startup.S
 * @brief Bootstraps the kernel.
 * @author Herbie Rand
 */

/**
 * @brief Entry-point routine first called by the bootrom.
 *
 * This entry point will be located at the binary's the lowest address.
 * It is the first thing the bootrom will execute after interpreting the
 * metadata in the image definition.
 */
.section .reset, "ax"
.global _entry_point
_entry_point:
    j _reset_handler

_reset_handler:
    // send core1 to jail if executing
    csrr a0, mhartid
    bnez a0, jail

    // initialize the stack pointer
    la sp, __stack0_base

    // for now, we'll use mscratch content to detect nested exceptions
    csrw mscratch, zero

    // mount the vector table
    la a0, __vector_table
    ori a0, a0, 1
    csrw mtvec, a0

    // initialize xip, load secondary bootloader from flash if necessary
    // BOOTRAM_BASE = 0x400e0000
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

    // execute xip setup function
    jalr sp
    addi sp, sp, 256

    // init .data section (copy from flash to RAM)
    la a0, __data_load_start
    la a1, __data_start
    la a2, __data_end
copy_data:
    beq a1, a2, copy_data_end
    lw a3, (a0)
    sw a3, (a1)
    addi a0, a0, 4
    addi a1, a1, 4
    j copy_data
copy_data_end:

    // zero .bss section
    la a0, __bss_start
    la a1, __bss_end
zero_bss:
    beq a0, a1, zero_bss_end
    sw zero, (a0)
    addi a0, a0, 4
    j zero_bss
zero_bss_end:

    // clear all IRQ force array bits
    // 32 iters * 16 bits = 512 bits cleared.
    // NOTE: MEIFA offset = 0xbe2
    li a0, 32
clear_meip:
    csrw 0xbe2u, a0 
    addi a0, a0, -1
    bgtz a0, clear_meip
    
    // enable external interrupts

    // enable mie.meie
    // NOTE: 0x800u = cause 11 (MEIE)
    li a0, 0x800u
    csrw mie, a0

    // enable mstatus.mie
    // NOTE: 0x8u = Bit 3 (represents mie in mstatus)
    csrsi mstatus, 0x8u

    // TODO: move these
    // test machine external interrupts:
    // j test_mei_irq0
    
    // test software interrupts:
    // j test_msi

    jal main

    // program ends
    ebreak

jail:
    // loop and breakpoint indefinitely
    ebreak
    j jail


/**
 * @brief Tests external interrupts (cause 11, IRQ 1)
 * TODO: move this into a test file or directory, or delete it.
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

    // We shouldn't make it this far, so go to jail
    j jail


/**
 * @brief Tests software interrupts (cause 3, set by RISSCV_SOFTIRQ)
 * TODO: move this into a test file or directory or delete it.
 */
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

/**
 * @brief Vector table, used for trap handling.
 * Note that in 'Vectored' mode, the vector table must be 64 byte aligned.
 * See rp2350 datasheet section 3.8.4.2.1
 * See riscv-privileged-20211203 section 3.1.7
 */
.p2align 6
.section .vectors, "ax"
__vector_table:

// disable compressed instructions
.option push
.option norvc
.option norelax

j isr_exc      // BASE +  0 
.word 0        // BASE +  4 
.word 0        // BASE +  8 
j isr_msi      // BASE + 12 
.word 0        // BASE + 16 
.word 0        // BASE + 20 
.word 0        // BASE + 24 
j isr_mti      // BASE + 28 
.word 0        // BASE + 32 
.word 0        // BASE + 36 
.word 0        // BASE + 40 
j isr_mei      // BASE + 44 
 
.option pop

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

    // NOTE: it is up to the handler to change mepc if
    // the faulting address should not be re-executed
    mret

.weak isr_msi

/**
 * @brief Handles machine software interrupts, triggered by RISCV_SOFTIRQ.
 * This will usually execute when one core wants to interrupt the other.
 */
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

.weak isr_mti
isr_mti:
    ebreak
    j jail

.weak isr_mei
isr_mei:
    ebreak
    j jail

/**
 * @brief Exception table, for dispatching ISRs based on mcause for
 *        synchronous exceptions.
 * @see RISC-V Privileged ISA Spec 3.1.15 
 */
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

/**
 * @brief Weak definitions of exception ISRs.
 * Assume the debugger will help with breakpoints for now.
 */
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

weak_def isr_soft_irq

/**
 * @brief Initial IMAGE_DEF item.
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
