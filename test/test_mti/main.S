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
