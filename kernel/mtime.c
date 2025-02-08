#include "mtime.h"
#include "asm.h"
#include "rp2350.h"
#include "types.h"

/** @brief XOSC nominal frequency is 11 MHz */
#define XOSC_NOMINAL_MHZ 11

static mtime_cache_t cache;

void mtimer_enable() {
    clr_mip(MTI_MASK);
    set_mie(MTI_MASK);
}

// NOTE: assumes XOSC
int mtimer_start(uint32_t us) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint32_t coef = XOSC_NOMINAL_MHZ;

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

    AT(SIO_MTIME_CTRL) = 0;
    AT(SIO_MTIME) = 0;
    AT(SIO_MTIMEH) = 0;
    AT(SIO_MTIMECMP) = (uint32_t)-1;
    AT(SIO_MTIMECMPH) = hi;
    AT(SIO_MTIMECMP) = lo;
    AT(SIO_MTIME_CTRL) = 3;
    return 0;
}
