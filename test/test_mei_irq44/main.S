/**
 * @file main.S
 * @brief Sanity check for mei handler implementation.
 *
 * Doesn't obey calling conventions.
 *
 * @author Herbie Rand
 */

#define RVCSR_MEIEA         0xbe0
#define RVCSR_MEIFA         0xbe2

.section .text
/**
 * @brief Tests external interrupts (cause 11, IRQ 44)
 *
 * If this executes correctly, we should reach the breakpoint in the weak
 * definition for `isr_irq44`.
 */
.global main
main:
    /* Bit key for MEIEA: W=Window, R=Reserved, I=Index
     * WWWW WWWW WWWW WWWW RRRR RRRR RRRI IIII
     * 1000 0000 0000 0000 0000 0000 0000 0010
     */
    li a0, 0x10000002
    csrw RVCSR_MEIEA, a0
    csrw RVCSR_MEIFA, a0
    fence.i

    j _jail
