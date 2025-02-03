/**
 * @brief Tests core 1 initialization by running blinky with mtimer interrupt
 *        on core 1.
 *
 * Expected behavior is blinking LED, identical to test_blinky_interrupt.
 * However, core 0 should spin indefinitely, and core 1 should be executing
 * the mtimer interrupt handler.
 *
 * @author Herbie Rand
 */
#include "asm.h"
#include "gpio.h"
#include "mtime.h"
#include "rp2350.h"
#include "runtime.h"
#include "types.h"

#define LED_PIN 25

void blinky();
void isr_mtimer_irq();

extern uint32_t __vector_table;
extern uint32_t __mstack1_base;

uint32_t *vt = &__vector_table;
uint32_t *sp1 = &__mstack1_base;

static uint8_t on = 0;
static uint32_t us = 5000000;

int main() {
    init_core1((uint32_t)vt, (uint32_t)sp1, (uint32_t)blinky);
    while (1) {
        asm volatile("wfi");
    }
    return 0;
}

void blinky() {
    // enable external interrupts on core 1
    set_mstatus(MIE_MASK);
    clr_meifa();
    mtimer_enable();
    gpio_init(LED_PIN);

    if (mtimer_start(us)) {
        asm volatile("ebreak");
    }
    while (1) {
        asm volatile("wfi");
    }
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
