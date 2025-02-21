#ifndef CLOCK_H
#define CLOCK_H

#include "asm.h"
#include "rp2350.h"
#include "types.h"

/** Note sources are only relevant for CLK_SYS and CLK_REF */
#define CLK_REF_SRC_DEFAULT 0x2 // XOSC SRC
#define CLK_SYS_SRC_DEFAULT 0x1 // AUX SRC

#define CLK_GPOUT_AUXSRC_DEFAULT 0x6
#define CLK_REF_AUXSRC_DEFAULT   0x0
#define CLK_SYS_AUXSRC_DEFAULT   0x0
#define CLK_PERI_AUXSRC_DEFAULT  0x0
#define CLK_HSTX_AUXSRC_DEFAULT  0x0
#define CLK_USB_AUXSRC_DEFAULT   0x0
#define CLK_ADC_AUXSRC_DEFAULT   0x0

#define CLK_GPOUT_DIV_DEFAULT 0x10000
#define CLK_REF_DIV_DEFAULT   0x10000
#define CLK_SYS_DIV_DEFAULT   0x10000
#define CLK_PERI_DIV_DEFAULT  0x10000
#define CLK_HSTX_DIV_DEFAULT  0x10000
#define CLK_USB_DIV_DEFAULT   0x10000
#define CLK_ADC_DIV_DEFAULT   0x10000

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
 */
void clock_defaults_set();

/**
 * @brief Applies the provided configuration to CLK_SYS.
 * @param src Integer indicating high-level clock source
 * @param auxsrc Integer indicating specific aux clock source
 * @param div Integer value for divider register
 */
void clk_sys_config(uint32_t src, uint32_t auxsrc, uint32_t div);

/**
 * @brief Applies the provided configuration to CLK_REF.
 * @param src Integer indicating high-level clock source
 * @param auxsrc Integer indicating specific aux clock source
 * @param div Integer value for divider register
 */
void clk_ref_config(uint32_t src, uint32_t auxsrc, uint32_t div);

/**
 * @brief Applies the provided configuration to CLK_USB.
 * @param auxsrc Integer indicating specific aux clock source
 * @param div Integer value for divider register
 */
void clk_usb_config(uint32_t auxsrc, uint32_t div);

/**
 * @brief Applies the provided configuration to CLK_PERI.
 * @param auxsrc Integer indicating specific aux clock source
 * @param div Integer value for divider register
 */
void clk_peri_config(uint32_t auxsrc, uint32_t div);

/**
 * @brief Applies the provided configuration to CLK_ADC.
 * @param auxsrc Integer indicating specific aux clock source
 * @param div Integer value for divider register
 */
void clk_adc_config(uint32_t auxsrc, uint32_t div);

/**
 * @brief Applies the provided configuration to CLK_HSTX.
 * @param auxsrc Integer indicating specific aux clock source
 * @param div Integer value for divider register
 */
void clk_hstx_config(uint32_t auxsrc, uint32_t div);

/**
 * @brief Sets XOSC as clock source.
 * Adapted from SDK xosc initialization function.
 */
void xosc_init();

/**
 * @brief Initializes SYS pll.
 * @param refdiv Integer reference divisor
 * @param vcofreq Integer VCO frequency (hz)
 * @param postidv1 Integer post divider (should be higher than postdiv2)
 * @param postidv2 Integer post divider (secondary)
 * @see pico SDK `rp2_common/hardware_clocks/scripts/vcocalc.py`
 * @see rp2350 datasheet 8.6.3.2
 */
void pll_sys_init(uint32_t refdiv, uint32_t vcofreq, uint32_t postdiv1,
                  uint32_t postdiv2);

/**
 * @brief Initializes USB pll.
 * @param refdiv Integer reference divisor
 * @param vcofreq Integer VCO frequency (hz)
 * @param postidv1 Integer post divider (should be higher than postdiv2)
 * @param postidv2 Integer post divider (secondary)
 * @see pico SDK `rp2_common/hardware_clocks/scripts/vcocalc.py`
 * @see rp2350 datasheet 8.6.3.2
 */
void pll_usb_init(uint32_t refdiv, uint32_t vcofreq, uint32_t postdiv1,
                  uint32_t postdiv2);

#endif
