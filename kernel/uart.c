#include "uart.h"
#include "asm.h"
#include "gpio.h"
#include "rp2350.h"

#define BAUDRATE 115200

// For now, let's implement this with no FIFO or DMA, KISS
// hardcode instance uart0

void uart_init() {

    // get clock hz

    // reset
    // unreset
    // set translate clrf
    uint32_t baud = uart_set_baudrate(BAUDRATE);

    // set frame parameters

    // enable uart, tx, rx
    AT(UART0_UARTCR) = (UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE);
    //
    // set gpio pins for uart instance 0
    gpio_init_func(0, 0x2); // init GPIO0 to UART0_TX
    gpio_init_func(1, 0x2); // init GPIO1 to UART0_RX
}

void uart_put(char c) {
    // wait for TX FIFO to have space
    while (AT(UART0_UARTFR) & UARTFR_TXFF)
        ;
    // write char to TX FIFO
    // AT(UART0_UARTDR) = c;
}

char uart_get() {
    // wait for RX FIFO to have a byte
    while (AT(UART0_UARTFR) & UARTFR_RXFE)
        ;
    // pop char from RX FIFO
}

#define UART_CLOCK_HZ 125000000

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
            ; // idk...
    }
    AT(UART0_UARTLCR_H) = 0;
    AT(UART0_UARTCR) = cr_save;

    return (4 * UART_CLOCK_HZ) / (64 * baud_ibrd + baud_fbrd);
}
