#include "clock.h"
#include "asm.h"
#include "resets.h"

static void _refsys_config(uint32_t rctrl, uint32_t rselected, uint32_t rdiv,
                           uint32_t src, uint32_t auxsrc, uint32_t div);

static void _nonsys_config(uint32_t rctrl, uint32_t rselected, uint32_t rdiv,
                           uint32_t auxsrc, uint32_t div);

static void _pll_init(uint32_t rcs, uint32_t rfbdiv, uint32_t rprim,
                      uint32_t rpwr, void (*reset)(), uint32_t refdiv,
                      uint32_t vcofreq, uint32_t postdiv1, uint32_t postdiv2);

// Initializes high-precision clocks for CLK_SYS, CLK_REF, CLK_PERI...
// Adapted from SDK and datasheet
void clock_defaults_set() {
    AT(CLOCKS_CLK_SYS_RESUS_CTRL) = 0;

    // Enable the xosc
    xosc_init();

    // Before we touch PLLs, switch sys and ref cleanly away from their aux
    // sources. switch CLK_SYS, CLK_REF away from AUX clocks if required
    AT(CLOCKS_CLK_SYS_CTRL + ATOMIC_BITCLR_OFFSET) = 0x3;
    while (AT(CLOCKS_CLK_SYS_SELECTED) != 0x1)
        ;

    AT(CLOCKS_CLK_REF_CTRL + ATOMIC_BITCLR_OFFSET) = 0x3;
    while (AT(CLOCKS_CLK_REF_SELECTED) != 0x1)
        ;

    pll_sys_init(PLL_SYS_REFDIV, PLL_SYS_VCO_FREQ_HZ, PLL_SYS_POSTDIV1,
                 PLL_SYS_POSTDIV2);
    pll_usb_init(PLL_USB_REFDIV, PLL_USB_VCO_FREQ_HZ, PLL_USB_POSTDIV1,
                 PLL_USB_POSTDIV2);

    clkref_config(CLK_REF_SRC_DEFAULT, CLK_REF_AUXSRC_DEFAULT,
                  CLK_REF_DIV_DEFAULT);
    clksys_config(CLK_SYS_SRC_DEFAULT, CLK_SYS_AUXSRC_DEFAULT,
                  CLK_SYS_DIV_DEFAULT);
    clkusb_config(CLK_USB_AUXSRC_DEFAULT, CLK_USB_DIV_DEFAULT);
    clkadc_config(CLK_ADC_AUXSRC_DEFAULT, CLK_ADC_DIV_DEFAULT);

    // ...etc
}

void clksys_config(uint32_t src, uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (src > 1 || auxsrc > 5) {
        breakpoint();
    }

    _refsys_config(CLOCKS_CLK_SYS_CTRL, CLOCKS_CLK_SYS_SELECTED,
                   CLOCKS_CLK_SYS_DIV, src, auxsrc, div);
}

void clkref_config(uint32_t src, uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (src > 3 || auxsrc > 3) {
        breakpoint();
    }

    _refsys_config(CLOCKS_CLK_REF_CTRL, CLOCKS_CLK_REF_SELECTED,
                   CLOCKS_CLK_REF_DIV, src, auxsrc, div);
}

void clkusb_config(uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (auxsrc > 6) {
        breakpoint();
    }

    _nonsys_config(CLOCKS_CLK_USB_CTRL, CLOCKS_CLK_USB_SELECTED,
                   CLOCKS_CLK_USB_DIV, auxsrc, div);
}

void clkperi_config(uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (auxsrc > 6) {
        breakpoint();
    }

    _nonsys_config(CLOCKS_CLK_USB_CTRL, CLOCKS_CLK_USB_SELECTED,
                   CLOCKS_CLK_USB_DIV, auxsrc, div);
}

void clkadc_config(uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (auxsrc > 5) {
        breakpoint();
    }
    _nonsys_config(CLOCKS_CLK_USB_CTRL, CLOCKS_CLK_USB_SELECTED,
                   CLOCKS_CLK_USB_DIV, auxsrc, div);
}

void xosc_init() {
    uint32_t xosc_ctrl;

    // configure XOSC
    AT(XOSC_CTRL) = XOSC_CTRL_DISABLE_VALUE | XOSC_1_15MHZ_RANGE;
    AT(XOSC_STARTUP) = XOSC_STARTUP_DELAY;

    // enable XOSC
    xosc_ctrl = AT(XOSC_CTRL) & ~XOSC_CTRL_ENABLE_BITS;
    AT(XOSC_CTRL) = xosc_ctrl | XOSC_CTRL_ENABLE_VALUE;

    // block until XOSC is stable
    while (!(AT(XOSC_STATUS) & XOSC_STATUS_STABLE))
        ;
}

void pll_sys_init(uint32_t refdiv, uint32_t vcofreq, uint32_t postdiv1,
                  uint32_t postdiv2) {
    _pll_init(PLL_SYS_CS, PLL_SYS_FBDIV_INT, PLL_SYS_PRIM, PLL_SYS_PWR,
              pll_sys_reset_cycle, refdiv, vcofreq, postdiv1, postdiv2);
}

void pll_usb_init(uint32_t refdiv, uint32_t vcofreq, uint32_t postdiv1,
                  uint32_t postdiv2) {
    _pll_init(PLL_USB_CS, PLL_USB_FBDIV_INT, PLL_USB_PRIM, PLL_USB_PWR,
              pll_usb_reset_cycle, refdiv, vcofreq, postdiv1, postdiv2);
}

// Some comments from SDK. Adapted from SDK
static void _refsys_config(uint32_t rctrl, uint32_t rselected, uint32_t rdiv,
                           uint32_t src, uint32_t auxsrc, uint32_t div) {
    // If increasing divisor, set divisor before source. Otherwise set source
    // before divisor. This avoids a momentary overspeed when e.g. switching
    // to a faster source and increasing divisor to compensate.
    if (div > AT(rdiv)) {
        AT(rdiv) = div;
    }

    // If switching a glitchless slice (ref or sys) to an aux source, switch
    // away from aux *first* to avoid passing glitches when changing aux mux.
    // Assume (!!!) glitchless source 0 is no faster than the aux source.
    if (src == 0x1) {
        AT(rctrl + ATOMIC_BITCLR_OFFSET) = 0x3;
        while (!(AT(rselected) & 0x1))
            ;
    }

    // Set aux mux first, and then glitchless mux if this clock has one
    AT(rctrl + ATOMIC_BITSET_OFFSET) = (auxsrc << 5) | src;
    while (!(AT(rselected) & (1 << src)))
        ;

    // Now that the source is configured, we can trust that the user-supplied
    // divisor is a safe value.
    AT(rdiv) = div;
}

static void _nonsys_config(uint32_t rctrl, uint32_t rselected, uint32_t rdiv,
                           uint32_t auxsrc, uint32_t div) {
    // If increasing divisor, set divisor before source. Otherwise set source
    // before divisor. This avoids a momentary overspeed when e.g. switching
    // to a faster source and increasing divisor to compensate.
    if (div > AT(rdiv)) {
        AT(rdiv) = div;
    }

    // TODO: implement this shiet
    (void)rctrl;
    (void)rselected;
    (void)auxsrc;
    (void)div;
}

static void _pll_init(uint32_t rcs, uint32_t rfbdiv, uint32_t rprim,
                      uint32_t rpwr, void (*reset)(), uint32_t refdiv,
                      uint32_t vcofreq, uint32_t postdiv1, uint32_t postdiv2) {
    uint32_t reffreq;
    uint32_t fbdiv;
    uint32_t pdiv;
    uint32_t pll_cs;

    reffreq = XOSC_HZ / refdiv;
    fbdiv = vcofreq / reffreq;
    pdiv = (postdiv1 << 16) | (postdiv2 << 12);

    // bounds-check values
    if (vcofreq < PICO_PLL_VCO_MIN_FREQ_HZ || vcofreq > PICO_PLL_FREQ_MAX_HZ) {
        breakpoint();
    }
    if (fbdiv < 16 || fbdiv > 320) {
        breakpoint();
    }
    if (postdiv1 < 1 || postdiv1 > 7 || postdiv2 < 1 || postdiv2 > 7) {
        breakpoint();
    }
    if (reffreq > (vcofreq / 16)) {
        breakpoint();
    }

    // check if desired configuration is already set
    if ((AT(rcs) & 0x1F == refdiv) && (AT(rfbdiv) == fbdiv) &&
        (AT(rprim) & PLL_PRIM_MASK == pdiv)) {
        breakpoint();
        return;
    }

    // reset PLL block
    reset();

    // set dividers
    AT(rcs) = refdiv;
    AT(rfbdiv) = fbdiv;

    // power on PLL (VOCPD, PD)
    AT(rpwr + ATOMIC_BITCLR_OFFSET) = (1 << 5) | 1;

    // wait for PLL to lock
    while (!(AT(rcs) & PLL_CS_LOCK_MASK))
        ;

    // setup post dividers
    AT(rprim) = pdiv;
    // (clear POSTDIV power down bit)
    AT(rpwr + ATOMIC_BITCLR_OFFSET) = (1 << 0x3);
}
