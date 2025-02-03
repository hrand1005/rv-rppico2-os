/**
 * @brief Defines asm helper functions.
 * Used for things that are easier to write in assmebly, e.g. csr operations.
 * @author Herbie Rand
 */

#include "rp2350.h"

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

.global set_mstatus
set_mstatus:
    csrs mstatus, a0
    ret

.global clr_mip
clr_mip:
    csrc mip, a0
    ret

.global clr_meifa
clr_meifa:
    // clear all IRQ force array bits
    // 4 iters * 16 bits = 64 bits cleared.
    li a0, 4
__meifa_loop:
    csrw RVCSR_MEIFA, a0 
    addi a0, a0, -1
    bgtz a0, __meifa_loop
    ret

.global clr_mstatus
clr_mstatus:
    csrc mstatus, a0
    ret
