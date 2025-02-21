#include "resets.h"
#include "asm.h"
#include "rp2350.h"

static __inline void _reset(uint32_t blocknum);
static __inline void _unreset(uint32_t blocknum);
static __inline void _unreset_blocking(uint32_t blocknum);

__inline void initial_reset_cycle() {
    AT(RESETS_RESET + ATOMIC_BITSET_OFFSET) =
        ~((1 << IO_QSPI_BLOCKNUM) | (1 << PADS_QSPI_BLOCKNUM) |
          (1 << PLL_USB_BLOCKNUM) | (1 << USBCTRL_BLOCKNUM) |
          (1 << SYSCFG_BLOCKNUM) | (1 << PLL_SYS_BLOCKNUM));

    uint32_t unreset_mask =
        (RESET_BITS &
         ~((1 << HSTX_BLOCKNUM) | (1 << ADC_BLOCKNUM) | (1 << SPI0_BLOCKNUM) |
           (1 << SPI1_BLOCKNUM) | (1 << UART0_BLOCKNUM) |
           (1 << UART1_BLOCKNUM) | (1 << USBCTRL_BLOCKNUM)));

    AT(RESETS_RESET + ATOMIC_BITCLR_OFFSET) = unreset_mask;
    while ((AT(RESETS_RESET_DONE) & unreset_mask) != unreset_mask)
        ;
}

__inline void postclk_reset_cycle() {
    AT(RESETS_RESET + ATOMIC_BITCLR_OFFSET) = RESET_BITS;
    while (AT(RESETS_RESET_DONE) != RESET_BITS)
        ;
}

__inline void pll_sys_reset_cycle() {
    _reset(PLL_SYS_BLOCKNUM);
    _unreset_blocking(PLL_SYS_BLOCKNUM);
}

__inline void pll_usb_reset_cycle() {
    _reset(PLL_USB_BLOCKNUM);
    _unreset_blocking(PLL_USB_BLOCKNUM);
}

__inline void uart_reset_cycle() {
    _reset(UART0_BLOCKNUM);
    _unreset_blocking(UART0_BLOCKNUM);
}

static __inline void _reset(uint32_t blocknum) {
    AT(RESETS_RESET + ATOMIC_BITSET_OFFSET) = (1 << blocknum);
}

static __inline void _unreset(uint32_t blocknum) {
    AT(RESETS_RESET + ATOMIC_BITCLR_OFFSET) = (1 << blocknum);
}

static __inline void _unreset_blocking(uint32_t blocknum) {
    _unreset(blocknum);
    while (!(AT(RESETS_RESET_DONE) & (1 << blocknum)))
        ;
}
