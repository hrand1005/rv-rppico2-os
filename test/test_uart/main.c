/**
 * @brief Tests UART with mtimer interrupt.
 *
 * The expected behavior is to see a new 'tick' (integer) each second.
 * Each tick is printed on a new line in the minicom console.
 * Tick overflows after max uint32 size.
 *
 * @author Herbie Rand
 */
#include "asm.h"
#include "clock.h"
#include "mtime.h"
#include "resets.h"
#include "types.h"
#include "uart.h"

void print_tick();

static uint32_t tick = 0;
static uint32_t us = 10000000;
static char buf[10];

int main() {
    initial_reset_cycle();
    clock_defaults_set();
    postclk_reset_cycle();
    uart_init();
    mtimer_enable();

    if (mtimer_start(us)) {
        asm volatile("ebreak");
    }

    while (1) {
        asm volatile("wfi");
    }
    return 0;
}

void print_tick() {
    int32_t i = 0;
    uint32_t num = tick;

    if (num == 0) {
        buf[i++] = 0 + '0';
    } else {
        while (num > 0) {
            buf[i++] = (num % 10) + '0';
            num = num / 10;
        }
    }

    while (i < 10) {
        buf[i++] = 0;
    }

    uart_putc('\n');
    uart_putc('\r');
    while (--i >= 0) {
        if (buf[i] == 0) {
            continue;
        }
        uart_putc(buf[i]);
    }
}

void isr_mtimer_irq() {
    mtimer_start(us);
    tick++;
    print_tick();
}
