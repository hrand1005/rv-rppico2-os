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
    la gp, __gp

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
    la a2, 0x400e0000
    
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
    // 4 iters * 16 bits = 64 bits cleared.
    // NOTE: MEIFA offset = 0xbe2
    li a0, 4
clear_meip:
    csrw 0xbe2, a0 
    addi a0, a0, -1
    bgtz a0, clear_meip

    // clear software interrupts in core0 and core1, if pending
clear_msip:
    // SIO_BASE                 = 0xd0000000
    // RISCV_SOFTIRQ Offset     = 0x1a0
    li a1, 0xd00001a0 
    li a2, 0x300
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
    
    // enable external interrupts

    // enable mie.meie
    // NOTE: 0x800 = cause 11 (MEIE)
    li a0, 0x800
    csrw mie, a0

    // enable mstatus.mie
    // NOTE: 0x8 = Bit 3 (represents mie in mstatus)
    csrsi mstatus, 0x8

    // test machine external interrupts:
    // j test_mei_irq0
    // j test_mei_irq44
    
    // test software interrupts:
    // j test_msi

    // test timer interrupts:
    // j test_mti

    jal main

    // program ends
    ebreak

jail:
    // loop and breakpoint indefinitely
    ebreak
    j jail


/**
 * @brief Tests external interrupts (cause 11, IRQ 0)
 * TODO: move this into a test file or directory, or delete it.
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


/**
 * @brief Tests software interrupts (cause 3, set by RISSCV_SOFTIRQ)
 * TODO: move this into a test file or directory or delete it.
 */
test_msi:
    // enable software interrupts (msie = 0x8 in mie)
    csrsi mie, 0x8

    // set the appropriate bit in RISCV_SOFTIRQ
    // SIO_BASE                 = 0xd0000000
    // RISCV_SOFTIRQ Offset     = 0x1a0
    li a1, 0xd00001a0 
    li a2, 1
    sw a2, 0(a1)

    fence.i
    j jail

/**
 * @brief Tests timer interrupts (cause 7).
 * TODO: move this into a test file or directory or delete it.
 */
test_mti:
    // enable timer interrupts (mtie = 0x80)
    li a0, 0x80
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
    li a1, 0x3
    sw a1, 0(a0)

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

/**
 * @brief Handles machine timer interrupts.
 */
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
    csrrsi a0, 0xbe4, 0x1
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
 * @brief Exception table, for dispatching IRQs by number for
 *        machine external interrupts.
 */
.p2align 2
.global __external_interrupt_table
__external_interrupt_table:
.word isr_irq0
.word isr_irq1
.word isr_irq2
.word isr_irq3
.word isr_irq4
.word isr_irq5
.word isr_irq6
.word isr_irq7
.word isr_irq8
.word isr_irq9
.word isr_irq10
.word isr_irq11
.word isr_irq12
.word isr_irq13
.word isr_irq14
.word isr_irq15
.word isr_irq16
.word isr_irq17
.word isr_irq18
.word isr_irq19
.word isr_irq20
.word isr_irq21
.word isr_irq22
.word isr_irq23
.word isr_irq24
.word isr_irq25
.word isr_irq26
.word isr_irq27
.word isr_irq28
.word isr_irq29
.word isr_irq30
.word isr_irq31
.word isr_irq32
.word isr_irq33
.word isr_irq34
.word isr_irq35
.word isr_irq36
.word isr_irq37
.word isr_irq38
.word isr_irq39
.word isr_irq40
.word isr_irq41
.word isr_irq42
.word isr_irq43
.word isr_irq44
.word isr_irq45
.word isr_irq46
.word isr_irq47
.word isr_irq48
.word isr_irq49
.word isr_irq50
.word isr_irq51

/**
 * @brief Weak definitions of exception ISRs.
 * Assume the debugger will help with breakpoints for now.
 */
.macro weak_def name
.weak \name
\name:
    ebreak
.endm

/* NOTE: exc dispatch */
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

// Catch-all ISR for unknown/unimplemented exception causes.
.global isr_unhandled_exc
isr_unhandled_exc:
    ebreak
    // for now, go to jail
    j jail

/* NOTE: msi dispatch */
weak_def isr_soft_irq

/* NOTE: mti dispatch */
weak_def isr_mtimer_irq

/* NOTE: mei dispatch */
weak_def isr_irq0
weak_def isr_irq1
weak_def isr_irq2
weak_def isr_irq3
weak_def isr_irq4
weak_def isr_irq5
weak_def isr_irq6
weak_def isr_irq7
weak_def isr_irq8
weak_def isr_irq9
weak_def isr_irq10
weak_def isr_irq11
weak_def isr_irq12
weak_def isr_irq13
weak_def isr_irq14
weak_def isr_irq15
weak_def isr_irq16
weak_def isr_irq17
weak_def isr_irq18
weak_def isr_irq19
weak_def isr_irq20
weak_def isr_irq21
weak_def isr_irq22
weak_def isr_irq23
weak_def isr_irq24
weak_def isr_irq25
weak_def isr_irq26
weak_def isr_irq27
weak_def isr_irq28
weak_def isr_irq29
weak_def isr_irq30
weak_def isr_irq31
weak_def isr_irq32
weak_def isr_irq33
weak_def isr_irq34
weak_def isr_irq35
weak_def isr_irq36
weak_def isr_irq37
weak_def isr_irq38
weak_def isr_irq39
weak_def isr_irq40
weak_def isr_irq41
weak_def isr_irq42
weak_def isr_irq43
weak_def isr_irq44
weak_def isr_irq45
weak_def isr_irq46
weak_def isr_irq47
weak_def isr_irq48
weak_def isr_irq49
weak_def isr_irq50
weak_def isr_irq51

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
