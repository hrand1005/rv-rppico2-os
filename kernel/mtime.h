#ifndef MTIME_H
#define MTIME_H

#include "rp2350.h"
#include "types.h"

/**
 * @brief Cache structure to prevent re-computing mtimecmph.
 *
 * May be helpful in case mtimecmph computations are expensive,
 * and/or the mtime counter is frequently reset.
 */
typedef struct {
    /** @brief Milliseconds (cache key) */
    uint32_t us;
    /** @brief Cached mtimecmp value */
    uint32_t mtimecmp;
    /** @brief Cached mtimecmph value */
    uint32_t mtimecmph;
} mtime_cache_t;

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

#endif
