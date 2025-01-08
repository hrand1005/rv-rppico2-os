#include "syscall.h"
#include "asm.h"
#include "gpio.h"
#include "rp2350.h"
#include "sys.h"
#include "types.h"

#define LED_PIN 25

static uint8_t led_init = 0;

static void (*syscall_table[])(exception_frame_t *) = {
    [SYS_LED_ON] sys_led_on,
    [SYS_LED_OFF] sys_led_off,
    [SYS_SPIN_MS] sys_spin_ms,
};

void isr_env_umode_exc(exception_frame_t *sf) {
    if (sf->a7 >= SYSCALL_COUNT) {
        // should never reach here
        breakpoint();
    }
    syscall_table[sf->a7](sf);
    inc_mepc();
}

void sys_led_on(exception_frame_t *sf) {
    (void)sf;
    if (!led_init) {
        gpio_init(LED_PIN);
        led_init = 1;
    }
    gpio_set(LED_PIN);
}

void sys_led_off(exception_frame_t *sf) {
    (void)sf;
    gpio_clr(LED_PIN);
}

void sys_spin_ms(exception_frame_t *sf) {
    uint32_t ms = (uint32_t)sf->a0;
    while (ms--) {
        // NOTE: inexact
        for (uint32_t j = 0; j < 17000; j++)
            ;
    }
}
