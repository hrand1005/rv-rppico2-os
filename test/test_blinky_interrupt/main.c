/**
 * @brief Tests blinky with mtimer interrupt.
 *
 * The actual rate of blinking depends on the clock source and environment.
 * However, assuming that the clock uses the default ROSC at nominal clock
 * rate of 11 MHz, `mtimer_start` will behave correctly, and the led will
 * blink on for 0.5 seconds, off for 0.5 seconds, repeatedly.
 *
 * @author Herbie Rand
 */
#include "asm.h"
#include "clock.h"
#include "gpio.h"
#include "mtime.h"
#include "types.h"

#define LED_PIN 25

static uint8_t on = 0;
static uint32_t us = 5000000;

int main() {
    clock_defaults_set();
    mtimer_enable();

    // breakpoint();

    gpio_init(LED_PIN);
    if (mtimer_start(us)) {
        asm volatile("ebreak");
    }
    while (1) {
        asm volatile("wfi");
    }
    return 0;
}

void isr_mtimer_irq() {
    if (!on) {
        gpio_set(LED_PIN);
    } else {
        gpio_clr(LED_PIN);
    }
    on = ~on;
    mtimer_start(us);
}
