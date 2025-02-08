#ifndef RESET_H
#define RESET_H

#define PLL_SYS_BLOCKNUM 14
#define PLL_USB_BLOCKNUM 15

/**
 * @brief Resets PLL SYS, then unresets, and waits for reset to finish.
 * You probably want to use this function.
 */
void pll_sys_reset_cycle();

/**
 * @brief Resets PLL USB, then unresets, and waits for reset to finish.
 * You probably want to use this function.
 */
void pll_usb_reset_cycle();

// ...etc.

#endif
