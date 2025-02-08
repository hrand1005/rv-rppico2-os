#include "resets.h"
#include "asm.h"
#include "rp2350.h"

static __inline void pll_sys_reset();
static __inline void pll_sys_unreset();
static __inline void pll_sys_unreset_blocking();

static __inline void pll_usb_reset();
static __inline void pll_usb_unreset();
static __inline void pll_usb_unreset_blocking();

__inline void pll_sys_reset_cycle() {
    pll_sys_reset();
    pll_sys_unreset_blocking();
}

static __inline void pll_sys_reset() {
    AT(RESETS_RESET + ATOMIC_BITSET_OFFSET) = (1 << PLL_SYS_BLOCKNUM);
}

static __inline void pll_sys_unreset() {
    AT(RESETS_RESET + ATOMIC_BITCLR_OFFSET) = (1 << PLL_SYS_BLOCKNUM);
}

static __inline void pll_sys_unreset_blocking() {
    pll_sys_unreset();
    while (!(AT(RESETS_RESET_DONE) & (1 << PLL_SYS_BLOCKNUM)))
        ;
}

__inline void pll_usb_reset_cycle() {
    pll_usb_reset();
    pll_usb_unreset_blocking();
}

static __inline void pll_usb_reset() {
    AT(RESETS_RESET + ATOMIC_BITSET_OFFSET) = (1 << PLL_USB_BLOCKNUM);
}

static __inline void pll_usb_unreset() {
    AT(RESETS_RESET + ATOMIC_BITCLR_OFFSET) = (1 << PLL_USB_BLOCKNUM);
}

static __inline void pll_usb_unreset_blocking() {
    pll_usb_unreset();
    while (!(AT(RESETS_RESET_DONE) & (1 << PLL_SYS_BLOCKNUM)))
        ;
}
