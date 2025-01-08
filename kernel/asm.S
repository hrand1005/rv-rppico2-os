/**
 * @brief Defines asm helper functions.
 * Used for things that are easier to write in assmebly, e.g. csr operations.
 * @author Herbie Rand
 */

.section .text
.global inc_mepc
inc_mepc:
    csrr t0, mepc
    addi t0, t0, 4
    csrw mepc, t0
    ret

.global breakpoint
breakpoint:
    ebreak
    ret

.global set_mie
set_mie:
    csrs mie, a0
    ret
