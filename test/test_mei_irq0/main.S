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
 * @brief Tests external interrupts (cause 11, IRQ 0).
 *
 * If this executes correctly, we should reach the breakpoint in the weak
 * definition for `isr_irq0`.
 */
.global main
main:
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
    csrw RVCSR_MEIEA, a0

    /* Next, we can create the interrupt request using MEIFA.
     * The register works the same as MEIA, except this time writing the
     * literal will force an interrupt.
     */
    csrw RVCSR_MEIFA, a0
    fence.i

    j _jail
