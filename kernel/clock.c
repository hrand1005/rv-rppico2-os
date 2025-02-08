#include "clock.h"
#include "asm.h"
#include "resets.h"

static uint32_t clock_frequency[] = {
    [ROSC_CLKSRC_PH] = 0,
    [CLKSRC_CLK_REF_AUX] = 0,
    [XOSC_CLKSRC] = 0,
    [LPOSC_CLKSRC] = 0,
};

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

    // CLK_REF
    // 0x2 --> XOSC source
    // 0x0 --> irrelevant aux source
    clkref_config(0x2, 0x0, CLK_REF_DIV_DEFAULT);

    // CLK_SYS
    // 0x1 --> AUX source
    // 0x0 --> PLL_SYS aux source
    clksys_config(0x1, 0x0, CLK_SYS_DIV_DEFAULT);

    // CLK_USB
    // 0x0 --> PLL_USB aux source
    clkusb_config(0x0, CLK_USB_DIV_DEFAULT);

    // CLK_ADC
    // 0x0 --> PLL_USB aux source
    clkadc_config(0x0, CLK_ADC_DIV_DEFAULT);

    // ...etc
}

uint32_t clksys_src() {
    // NOTE: one-hot encoding
    uint32_t sys_select = AT(CLOCKS_CLK_SYS_SELECTED);
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

// Some comments from SDK. Adapted from SDK
void clksys_config(uint32_t src, uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (src > 1 || auxsrc > 5) {
        breakpoint();
    }

    // If increasing divisor, set divisor before source. Otherwise set source
    // before divisor. This avoids a momentary overspeed when e.g. switching
    // to a faster source and increasing divisor to compensate.
    if (div > AT(CLOCKS_CLK_SYS_DIV)) {
        AT(CLOCKS_CLK_SYS_DIV) = div;
    }

    // If switching a glitchless slice (ref or sys) to an aux source, switch
    // away from aux *first* to avoid passing glitches when changing aux mux.
    // Assume (!!!) glitchless source 0 is no faster than the aux source.
    if (src == 0x1) {
        AT(CLOCKS_CLK_SYS_CTRL + ATOMIC_BITCLR_OFFSET) = 0x3;
        while (!(AT(CLOCKS_CLK_SYS_SELECTED) & 0x1))
            ;
    }

    // Set aux mux first, and then glitchless mux if this clock has one
    AT(CLOCKS_CLK_SYS_CTRL + ATOMIC_BITSET_OFFSET) = (auxsrc << 5) | src;
    while (!(AT(CLOCKS_CLK_SYS_SELECTED) & (1 << src)))
        ;

    // Now that the source is configured, we can trust that the user-supplied
    // divisor is a safe value.
    AT(CLOCKS_CLK_SYS_DIV) = div;
}

// Some comments from SDK. Adapted from SDK
void clkref_config(uint32_t src, uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (src > 3 || auxsrc > 3) {
        breakpoint();
    }

    // If increasing divisor, set divisor before source. Otherwise set source
    // before divisor. This avoids a momentary overspeed when e.g. switching
    // to a faster source and increasing divisor to compensate.
    if (div > AT(CLOCKS_CLK_REF_DIV)) {
        AT(CLOCKS_CLK_REF_DIV) = div;
    }

    // If switching a glitchless slice (ref or sys) to an aux source, switch
    // away from aux *first* to avoid passing glitches when changing aux mux.
    // Assume (!!!) glitchless source 0 is no faster than the aux source.
    if (src == 0x1) {
        AT(CLOCKS_CLK_REF_CTRL + ATOMIC_BITCLR_OFFSET) = 0x3;
        while (!(AT(CLOCKS_CLK_REF_SELECTED) & 0x1))
            ;
    }

    // Set aux mux first, and then glitchless mux if this clock has one
    AT(CLOCKS_CLK_REF_CTRL + ATOMIC_BITSET_OFFSET) = (auxsrc << 5) | src;
    while (!(AT(CLOCKS_CLK_REF_SELECTED) & (1 << src)))
        ;

    // Now that the source is configured, we can trust that the user-supplied
    // divisor is a safe value.
    AT(CLOCKS_CLK_REF_DIV) = div;
}

void clkusb_config(uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (auxsrc > 6) {
        breakpoint();
    }

    // If increasing divisor, set divisor before source. Otherwise set source
    // before divisor. This avoids a momentary overspeed when e.g. switching
    // to a faster source and increasing divisor to compensate.
    if (div > AT(CLOCKS_CLK_USB_DIV)) {
        AT(CLOCKS_CLK_USB_DIV) = div;
    }

    // ...not implemented :|
}

void clkperi_config(uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (auxsrc > 6) {
        breakpoint();
    }

    // If increasing divisor, set divisor before source. Otherwise set source
    // before divisor. This avoids a momentary overspeed when e.g. switching
    // to a faster source and increasing divisor to compensate.
    if (div > AT(CLOCKS_CLK_PERI_DIV)) {
        AT(CLOCKS_CLK_PERI_DIV) = div;
    }

    // ...not implemented :|
}

void clkadc_config(uint32_t auxsrc, uint32_t div) {
    // src, auxsrc bounds checking
    if (auxsrc > 5) {
        breakpoint();
    }

    // If increasing divisor, set divisor before source. Otherwise set source
    // before divisor. This avoids a momentary overspeed when e.g. switching
    // to a faster source and increasing divisor to compensate.
    if (div > AT(CLOCKS_CLK_ADC_DIV)) {
        AT(CLOCKS_CLK_ADC_DIV) = div;
    }

    // ...not implemented :|
}

uint32_t clkref_src() {
    // NOTE: one-hot encoding
    uint32_t ref_select = AT(CLOCKS_CLK_REF_SELECTED);
    switch (ref_select) {
    case 1: // ROSC_CLKSRC_PH
        breakpoint();
        break;
    case 2: // CLKSRC_CLK_REF_AUX
        breakpoint();
        break;
    case 4: // XOSC_CLKSRC
        breakpoint();
        break;
    case 8: // LPOSC_CLKSRC
        breakpoint();
        break;
    }
    return 0;
}

void xosc_init() {
    uint32_t xosc_ctrl;

    // configure XOSC
    AT(XOSC_CTRL) |= XOSC_1_15MHZ_RANGE;
    AT(XOSC_STARTUP) = XOSC_STARTUP_DELAY;

    // enable XOSC
    xosc_ctrl = AT(XOSC_CTRL) & ~XOSC_CTRL_ENABLE_BITS;
    AT(XOSC_CTRL + ATOMIC_BITSET_OFFSET) = xosc_ctrl | XOSC_CTRL_ENABLE_VALUE;

    // block until XOSC is stable
    while (!(AT(XOSC_STATUS) & XOSC_STATUS_STABLE))
        ;
}

void pll_sys_init(uint32_t refdiv, uint32_t vcofreq, uint32_t postdiv1,
                  uint32_t postdiv2) {
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
    if ((AT(PLL_SYS_CS) & 0x1F == refdiv) && (AT(PLL_SYS_FBDIV_INT) == fbdiv) &&
        (AT(PLL_SYS_PRIM) & PLL_PRIM_MASK == pdiv)) {
        breakpoint();
        return;
    }

    // reset PLL block
    pll_sys_reset_cycle();

    // set dividers
    AT(PLL_SYS_CS) = refdiv;
    AT(PLL_SYS_FBDIV_INT) = fbdiv;

    // power on PLL (VOCPD, PD)
    AT(PLL_SYS_PWR + ATOMIC_BITCLR_OFFSET) = (1 << 5) | 1;

    // wait for PLL to lock
    while (!(AT(PLL_SYS_CS) & PLL_CS_LOCK_MASK))
        ;

    // setup post dividers
    AT(PLL_SYS_PRIM) = pdiv;
    // (clear POSTDIV power down bit)
    AT(PLL_SYS_PWR + ATOMIC_BITCLR_OFFSET) = (1 << 3);

    return;
}

void pll_usb_init(uint32_t refdiv, uint32_t vcofreq, uint32_t postdiv1,
                  uint32_t postdiv2) {
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
    if ((AT(PLL_USB_CS) & 0x1F == refdiv) && (AT(PLL_USB_FBDIV_INT) == fbdiv) &&
        (AT(PLL_USB_PRIM) & PLL_PRIM_MASK == pdiv)) {
        breakpoint();
        return;
    }

    // reset PLL block
    pll_usb_reset_cycle();

    // set dividers
    AT(PLL_USB_CS) = refdiv;
    AT(PLL_USB_FBDIV_INT) = fbdiv;

    // power on PLL (VOCPD, PD)
    AT(PLL_USB_PWR + ATOMIC_BITCLR_OFFSET) = (1 << 5) | 1;

    // wait for PLL to lock
    while (!(AT(PLL_USB_CS) & PLL_CS_LOCK_MASK))
        ;

    // setup post dividers
    AT(PLL_USB_PRIM) = pdiv;
    // (clear POSTDIV power down bit)
    AT(PLL_USB_PWR + ATOMIC_BITCLR_OFFSET) = (1 << 0x3);

    return;
}
