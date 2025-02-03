#include "mtime.h"
#include "asm.h"
#include "rp2350.h"
#include "types.h"

/** @brief ROSC nominal frequency is 11 MHz */
#define ROSC_NOMINAL_MHZ 11

static mtime_cache_t cache;

void mtimer_enable() {
    clr_mip(MTI_MASK);
    set_mie(MTI_MASK);
}

// NOTE: assumes ROSC
int mtimer_start(uint32_t us) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint32_t coef = ROSC_NOMINAL_MHZ;

    if (us == cache.us) {
        lo = cache.mtimecmp;
        hi = cache.mtimecmph;
    } else {
        // safe 32-bit * 32-bit multiplication
        // note that exceeding 64 bits is impossible
        while (coef--) {
            if (lo + us < lo) {
                hi++;
            }
            lo += us;
        }
        cache.us = us;
        cache.mtimecmp = lo;
        cache.mtimecmph = hi;
    }

    *(uint32_t *)SIO_MTIME_CTRL = 0;
    *(uint32_t *)SIO_MTIME = 0;
    *(uint32_t *)SIO_MTIMEH = 0;
    *(uint32_t *)SIO_MTIMECMP = (uint32_t)-1;
    *(uint32_t *)SIO_MTIMECMPH = hi;
    *(uint32_t *)SIO_MTIMECMP = lo;
    *(uint32_t *)SIO_MTIME_CTRL = 3;
    return 0;
}

int _clksys_src() {
    uint32_t sys_select = *(uint32_t *)CLOCKS_CLK_SYS_SELECTED;
    switch (sys_select) {
    case 1: // CLK_REF
        breakpoint();
        break;
    case 2: // CLKSRC_CLK_SYS_AUX
        breakpoint();
        break;
    }
    return 0;
}

int _clkref_src() {
    uint32_t ref_select = *(uint32_t *)CLOCKS_CLK_REF_SELECTED;
    switch (ref_select) {
    case 1: // ROSC_CLKSRC_PH
        breakpoint();
        break;
    case 2: // CLKSRC_CLK_REF_AUX
        breakpoint();
        break;
    case 3: // XOSC_CLKSRC
        breakpoint();
        break;
    case 4: // LPOSC_CLKSRC
        breakpoint();
        break;
    }
    return 0;
}
