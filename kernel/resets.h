#ifndef RESET_H
#define RESET_H

#define PLL_SYS_BLOCKNUM 14
#define PLL_USB_BLOCKNUM 15
#define UART0_BLOCKNUM   26
#define UART1_BLOCKNUM   27

/**
 * @brief Resets PLL SYS, then unresets, blocking until complete.
 */
void pll_sys_reset_cycle();

/**
 * @brief Resets PLL USB, then unresets, blocking until complete.
 */
void pll_usb_reset_cycle();

/**
 * @brief Resets UART (instance 0), then unresets, blocking until complete.
 */
void uart_reset_cycle();

#endif
