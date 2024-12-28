#ifndef MTIME_H
#define MTIME_H

#include "rp2350.h"
#include "types.h"

/**
 * @brief Enables the mtime timer interrupt.
 * You can implement the interrupt handler by overriding
 * the weak definition for `void isr_mtimer_irq()`.
 */
void mtimer_enable();

/**
 * @brief Starts the RISC-V mtime timer, interrupting at the provided duration.
 * @param us    Integer microseconds indicating duration before interrupt.
 * @return 0 on success, nonzero on error
 */
int mtimer_start(uint32_t us);

/**
 * @brief Stops the mtime timer from ticking.
 */
// void mtimer_stop();

/**
 * @brief Detects source of system clock.
 * @return Integer indicating system clock source
 */
int _clksys_src();

/**
 * @brief Detects source of reference clock.
 * @return Integer indicating reference clock source
 */
int _clkref_src();

#endif
