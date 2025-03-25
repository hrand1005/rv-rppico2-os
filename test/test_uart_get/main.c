/**
 * @brief Tests reading from UART.
 *
 * Echos contents written to the UART console. Reads until '\r', then echos.
 * Or until the buffer fills up. Whichever happens first.
 *
 * @author Herbie Rand
 */
#include "asm.h"
#include "clock.h"
#include "mtime.h"
#include "resets.h"
#include "types.h"
#include "uart.h"

#define BUFSIZE 100

void echo(char *);
void print(char *, uint32_t);

static const char *msg = "echoing:\n\t";
static char buf[BUFSIZE];

int main() {
    initial_reset_cycle();
    clock_defaults_set();
    postclk_reset_cycle();
    uart_init();

    char c;
    uint32_t i = 0;

    while (1) {
        c = uart_getc();
        if (c == '\r' || i == 99) {
            buf[i] = '\0';
            echo(buf);
            i = 0;
        } else {
            buf[i++] = c;
        }
    }
    return 0;
}

void echo(char *buf) {
    uint32_t i = 0;
    uart_putc('\r');
    while (msg[i] != '\0') {
        uart_putc(msg[i++]);
    }
    print(buf, BUFSIZE);
}

void print(char *buf, uint32_t n) {
    uint32_t i = 0;
    while (i < n && buf[i] != '\0') {
        uart_putc(buf[i++]);
    }
    uart_putc('\n');
    uart_putc('\r');
}
