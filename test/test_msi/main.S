/**
 * @file main.S
 * @brief Sanity check for msi handler implementation.
 *
 * Doesn't obey calling conventions.
 *
 * @author Herbie Rand
 */

#define SIO_RISCV_SOFTIRQ   0xd00001a0

.section .text
/**
 * @brief Tests software interrupts (cause 3, set by RISSCV_SOFTIRQ)
 * If this executes correctly, we should reach the breakpoint in the weak
 * definition for `isr_soft_irq`
 */
.global main
main:
    // enable mie.msie
    csrsi mie, 0x8

    // set the appropriate bit in RISCV_SOFTIRQ
    li a1, SIO_RISCV_SOFTIRQ
    li a2, 1
    sw a2, 0(a1)

    fence.i
    j _jail
