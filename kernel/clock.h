#ifndef CLOCK_H
#define CLOCK_H

#include "asm.h"
#include "rp2350.h"
#include "types.h"

#define ROSC_CLKSRC_PH     1
#define CLKSRC_CLK_REF_AUX 2
#define XOSC_CLKSRC        3
#define LPOSC_CLKSRC       4

#define CLK_REF_DIV_DEFAULT 0x10000
#define CLK_SYS_DIV_DEFAULT 0x10000
#define CLK_ADC_DIV_DEFAULT 0x0
#define CLK_USB_DIV_DEFAULT 0x0

/**
 * PLL_SYS Parameters computed by vcocalc.py.
 * Tuned for precision, not minimizing power consumption.
 * */
#define PLL_SYS_REFDIV      1
#define PLL_SYS_VCO_FREQ_HZ 1500000000
#define PLL_SYS_POSTDIV1    5
#define PLL_SYS_POSTDIV2    2

/**
 * PLL_USB Parameters computed by vcocalc.py.
 * Tuned for precision, not minimizing power consumption.
 * */
#define PLL_USB_REFDIV      1
#define PLL_USB_VCO_FREQ_HZ 1440000000
#define PLL_USB_POSTDIV1    6
#define PLL_USB_POSTDIV2    5

#define XOSC_MZ            12
#define XOSC_KZ            12000
#define XOSC_HZ            12000000
#define XOSC_STARTUP_DELAY (((XOSC_KZ) + 128) / 256)
#define XOSC_1_15MHZ_RANGE 0xaa0

#define PICO_PLL_VCO_MIN_FREQ_HZ 756000000
#define PICO_PLL_FREQ_MAX_HZ     1596000000

/**
 * @brief Sets up default clock sources for CLK_SYS, CLK_REF, CLK_USB, etc.
 *
 */
void clock_defaults_set();

void clksys_config(uint32_t src, uint32_t auxsrc, uint32_t div);
void clkref_config(uint32_t src, uint32_t auxsrc, uint32_t div);

void clkadc_config(uint32_t auxsrc, uint32_t div);
void clkusb_config(uint32_t auxsrc, uint32_t div);
void clkperi_config(uint32_t auxsrc, uint32_t div);

/**
 * @brief Detects source of system clock.
 * @return Integer indicating system clock source
 */
uint32_t clksys_src();

/**
 * @brief Detects source of reference clock.
 * @return Integer indicating reference clock source
 */
uint32_t clkref_src();

/**
 * @brief Detects source of peripheral clock.
 * @return Integer indicating peripheral clock source
 */
uint32_t clkperi_src();

/**
 * @brief Sets XOSC as clock source.
 * Adapted from SDK xosc initialization function.
 */
void xosc_init();

/**
 * @brief Initializes pll with provided parameters.
 *
 * @see pico SDK `rp2_common/hardware_clocks/scripts/vcocalc.py`
 * @see rp2350 datasheet 8.6.3.2
 */
void pll_sys_init(uint32_t refdiv, uint32_t vcofreq, uint32_t postdiv1,
                  uint32_t postdiv2);

/**
 * @brief Initializes pll with provided parameters.
 *
 * Effective parameters for a target operating frequency may be determined
 * using
 *
 * @see pico SDK `rp2_common/hardware_clocks/scripts/vcocalc.py`
 * @see rp2350 datasheet 8.6.3.2
 */
void pll_usb_init();

#endif
