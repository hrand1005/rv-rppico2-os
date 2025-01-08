/**
 * @brief Contains syscall prototypes.
 */
#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

/** @brief Exception frame, pushed onto the stack during ecall */
typedef struct {
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
} exception_frame_t;

/**
 * @brief Overrides weak umode ecall service routine.
 * @param exception_frame_t containing syscall args
 */
void isr_env_umode_exc(exception_frame_t *);

/**
 * @brief Uses GPIO to turn on the LED.
 * @param exception_frame_t containing syscall args
 */
void sys_led_on(exception_frame_t *);

/**
 * @brief Uses GPIO to turn off the LED.
 * @param exception_frame_t containing syscall args
 */
void sys_led_off(exception_frame_t *);

/**
 * @brief Spins roughly specified number of milliseconds.
 * @param exception_frame_t containing syscall args
 */
void sys_spin_ms(exception_frame_t *);

#endif
