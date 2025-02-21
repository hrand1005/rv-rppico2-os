#ifndef RESET_H
#define RESET_H

#define RESET_BITS 0x1fffffff

#define ADC_BLOCKNUM        0
#define BUSCTRL_BLOCKNUM    1
#define DMA_BLOCKNUM        2
#define HSTX_BLOCKNUM       3
#define I2C0_BLOCKNUM       4
#define I2C1_BLOCKNUM       5
#define IO_BANK0_BLOCKNUM   6
#define IO_QSPI_BLOCKNUM    7
#define JTAG_BLOCKNUM       8
#define PADS_BANK0_BLOCKNUM 9
#define PADS_QSPI_BLOCKNUM  10
#define PIO0_BLOCKNUM       11
#define PIO1_BLOCKNUM       12
#define PIO2_BLOCKNUM       13
#define PLL_SYS_BLOCKNUM    14
#define PLL_USB_BLOCKNUM    15
#define PWM_BLOCKNUM        16
#define SHA256_BLOCKNUM     17
#define SPI0_BLOCKNUM       18
#define SPI1_BLOCKNUM       19
#define SYSCFG_BLOCKNUM     20
#define SYSINFO_BLOCKNUM    21
#define TBMAN_BLOCKNUM      22
#define TIMER0_BLOCKNUM     23
#define TIMER1_BLOCKNUM     24
#define TRNG_BLOCKNUM       25
#define UART0_BLOCKNUM      26
#define UART1_BLOCKNUM      27
#define USBCTRL_BLOCKNUM    28

/**
 * @brief Resets most peripherals to a known state. Call this at startup.
 */
void initial_reset_cycle();

/**
 * @brief Resets most peripherals to a known state, to be called after clock
 *        configuration.
 */
void postclk_reset_cycle();

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
