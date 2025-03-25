#include "uart.h"
#include "asm.h"
#include "gpio.h"
#include "resets.h"
#include "rp2350.h"

#define BAUDRATE 115200

static __inline void _uart_set_default_format();

void uart_init() {
    // set uart functions on GPIO0 and GPIO1, and remove pad isolation control
    gpio_set_func(0, 2);
    gpio_set_func(1, 2);

    uart_reset_cycle();

    // TODO: set translate clrf, if necessary
    uint32_t baud = uart_set_baudrate(BAUDRATE);

    // set frame parameters
    _uart_set_default_format();

    // enable uart, tx, rx
    AT(UART0_UARTCR) = (UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE);

    // TODO: enable FIFOs (UARTLCR_H)

    // TODO: enable DMA requests

    (void)baud;
}

void uart_putc(char c) {
    // wait for TX FIFO to have space
    while (AT(UART0_UARTFR) & UARTFR_TXFF)
        ;
    AT(UART0_UARTDR) = c;
}

char uart_getc() {
    // wait for RX FIFO to have a byte
    while (AT(UART0_UARTFR) & UARTFR_RXFE)
        ;
    // TODO: check for errors
    return AT(UART0_UARTDR) & 0xff;
}

#define UART_CLOCK_HZ 150000000

// adapted from datasheet 12.1.7.1
uint32_t uart_set_baudrate(uint32_t baudrate) {
    uint32_t baudrate_div = (8 * UART_CLOCK_HZ / baudrate) + 1;
    uint32_t baud_ibrd = baudrate_div >> 7;
    uint32_t baud_fbrd;
    uint32_t cr_save;

    if (baud_ibrd == 0) {
        baud_ibrd = 1;
        baud_fbrd = 0;
    } else if (baud_ibrd >= 65535) {
        baud_ibrd = 65535;
        baud_fbrd = 0;
    } else {
        baud_fbrd = (baudrate_div & 0x7f) >> 1;
    }

    AT(UART0_UARTIBRD) = baud_ibrd;
    AT(UART0_UARTFBRD) = baud_fbrd;

    // write to LCR_H to "latch in the divisors"
    cr_save = AT(UART0_UARTCR);

    // insert delay, if outstanding transmit/receive
    if (cr_save & UARTCR_UARTEN) {
        breakpoint();

        for (uint32_t i = 0; i < 10000; i++)
            ;
    }
    AT(UART0_UARTLCR_H) = 0;
    AT(UART0_UARTCR) = cr_save;

    return (4 * UART_CLOCK_HZ) / (64 * baud_ibrd + baud_fbrd);
}

// See UARTLCR_H Documentation.
static __inline void _uart_set_default_format() {
    uint32_t wlen = 8;
    uint32_t stp2 = 1;
    uint32_t fen = 1;
    uint32_t pen = 0;
    uint32_t lcr =
        ((wlen - 5) << 5) | (fen << 4) | ((stp2 - 1) << 3) | (pen << 1);

    AT(UART0_UARTLCR_H + ATOMIC_BITSET_OFFSET) = lcr;
}
