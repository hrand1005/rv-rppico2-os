#include "mtime.h"
#include "asm.h"
#include "clock.h"
#include "rp2350.h"
#include "types.h"

static mtime_cache_t cache;

void mtimer_enable() {
    clr_mip(MTI_MASK);
    set_mie(MTI_MASK);
}

int mtimer_start(uint32_t us) {
    uint32_t lo = 0;
    uint32_t hi = 0;
    uint32_t coef = clk_ref_freq_mhz();

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

#include "clock.h"

// Some tests indicate that each loop takes ~5 cycles.
// Either that, or I have misconfigured something else.
// But the frequency counter indicates that the clock
// sources are configured correctly.
// See test/test_clk_frequency for details.
void spin_us(uint32_t us) {
    uint32_t mhz = clk_sys_freq_mhz();
    uint32_t spin = (mhz * us) / 5;
    while (spin--)
        ;
}
