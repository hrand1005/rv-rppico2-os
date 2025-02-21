#include "asm.h"
#include "clock.h"
#include "mtime.h"
#include "resets.h"
#include "types.h"
#include "uart.h"

static uint32_t half_second_us = 5000000;

int main() {
    initial_reset_cycle();
    clock_defaults_set();
    postclk_reset_cycle();
    uart_init();
    mtimer_enable();
    if (mtimer_start(half_second_us)) {
        asm volatile("ebreak");
    }

    while (1) {
        asm volatile("wfi");
    }
    return 0;
}

void isr_mtimer_irq() {
    uart_putc('X');
    mtimer_start(half_second_us);
}
