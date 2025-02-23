/**
 * @author Herbie Rand
 */
#include "asm.h"
#include "gpio.h"
#include "mtime.h"
#include "types.h"

#include "clock.h"
#include "resets.h"

#define LED_PIN 25

static uint32_t us = 500000;

int main() {
    initial_reset_cycle();
    clock_defaults_set();
    postclk_reset_cycle();
    gpio_init(LED_PIN);
    while (1) {
        gpio_set(LED_PIN);
        spin_us(us);
        gpio_clr(LED_PIN);
        spin_us(us);
    }
    return 0;
}
