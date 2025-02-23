/**
 * @brief Checks the real frequency of clocks.
 *
 * This can be used a sanity check that PLL and divider values result in
 * clocks being configured correctly with the desired frequencies.
 * Inspect khz and frac upon hitting the breakpoint.
 * You may also modify the clock to check (AT(FC0_SRC) line).
 *
 * @author Herbie Rand
 * @see Datasheet 8.1.5.2
 */
#include "asm.h"
#include "clock.h"
#include "resets.h"
#include "rp2350.h"
#include "types.h"
#include "uart.h"

#define FC0_REF_KHZ  (CLOCKS_BASE + 0x8c)
#define FC0_INTERVAL (CLOCKS_BASE + 0x9c)
#define FC0_SRC      (CLOCKS_BASE + 0xa0)
#define FC0_STATUS   (CLOCKS_BASE + 0xa4)
#define FC0_RESULT   (CLOCKS_BASE + 0xa8)

#define FC0_STATUS_DONE (1 << 4)

int main() {
    initial_reset_cycle();
    clock_defaults_set();
    postclk_reset_cycle();

    AT(FC0_REF_KHZ) = clk_ref_freq_mhz() * 1000;
    AT(FC0_INTERVAL) = 11;
    // start the test!
    // 1 --> PLL_SYS_CLKSRC_PRIMARY
    // 2 --> PLL_USB_CLKSRC_PRIMARY
    // 8 --> CLK_REF
    // 9 --> CLK_SYS
    // 0a --> CLK_PERI
    // 0b --> CLK_USB
    AT(FC0_SRC) = 0x9;
    while (!(AT(FC0_STATUS) & FC0_STATUS_DONE))
        ;

    uint32_t result = AT(FC0_RESULT);
    uint32_t khz = result >> 5;
    uint32_t frac = result & 0x1f;

    breakpoint();

    return 0;
}
