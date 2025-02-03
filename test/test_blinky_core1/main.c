#include "gpio.h"
#include "mtime.h"
#include "riscv.h"
#include "types.h"

#include "asm.h"

#define LED_PIN 25

void init_core1();
void multicore_fifo_drain();
void __sev();
void multicore_fifo_push_blocking(uint32_t);
uint32_t multicore_fifo_pop_blocking();

void blinky();
void isr_mtimer_irq();

extern uint32_t __vector_table;
extern uint32_t __mstack1_base;

uint32_t *vt = &__vector_table;
uint32_t *sp1 = &__mstack1_base;

static uint8_t on = 0;
static uint32_t us = 5000000;
static uint32_t cmd_sequence[6];

int main() {
    cmd_sequence[0] = 0;
    cmd_sequence[1] = 0;
    cmd_sequence[2] = 1;
    cmd_sequence[3] = (uint32_t)vt;
    cmd_sequence[4] = (uint32_t)sp1;
    cmd_sequence[5] = (uint32_t)blinky;
    init_core1();
    while (1) {
        asm volatile("wfi");
    }
    return 0;
}

void init_core1() {
    uint32_t cmd;
    uint32_t resp;

    uint32_t seq = 0;
    do {
        cmd = cmd_sequence[seq];
        if (!cmd) {
            // TODO: drain FIFO
            multicore_fifo_drain();
            // TODO: execute SEV on core 1
            __sev();
        }
        multicore_fifo_push_blocking(cmd);
        resp = multicore_fifo_pop_blocking();
        seq = (cmd == resp) ? (seq + 1) : 0;
    } while (seq < 6);
}

#define SIO_BASE    0xd0000000
#define SIO_FIFO_ST 0xd0000050
#define SIO_FIFO_WR 0xd0000054
#define SIO_FIFO_RD 0xd0000058

void multicore_fifo_drain() {
    uint32_t rd;
    // read until RX is empty
    while (*(uint32_t *)SIO_FIFO_ST & 0x1) {
        rd = *(uint32_t *)SIO_FIFO_RD;
    }
    (void)rd;
}

#define __h3_unblock() asm("slt x0, x0, x1")

void __sev() {
    __h3_unblock();
}

void multicore_fifo_push_blocking(uint32_t cmd) {
    while (!(*(uint32_t *)SIO_FIFO_ST & 0x2))
        ;
    *(uint32_t *)SIO_FIFO_WR = cmd;
    __sev();
}

uint32_t multicore_fifo_pop_blocking() {
    while (!(*(uint32_t *)SIO_FIFO_ST & 0x1))
        ;
    return *(uint32_t *)SIO_FIFO_RD;
}

void blinky() {
    breakpoint();
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
