#include "asm.h"
#include "clock.h"
#include "resets.h"
#include "types.h"
#include "uart.h"

int main() {
    initial_reset_cycle();
    clock_defaults_set();
    postclk_reset_cycle();
    uart_init();
    while (1) {
        uart_putc('X');
        for (uint32_t i = 0; i < 100000; i++)
            ;
    }
    return 0;
}
