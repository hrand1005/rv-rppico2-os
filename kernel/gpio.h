#ifndef GPIO_H
#define GPIO_H

#include "types.h"

/**
 * @brief Initializes GPIO on the selected GPIO pin.
 * Assumes SIO control function, and enables pin for output.
 * @param pin   Integer GPIO pin
 */
void gpio_init(uint32_t pin);

/**
 * @brief Initializes GPIO with provided function on the selected pin.
 *
 * @param pin       Integer GPIO pin
 * @param funcsel   Integer function select
 */
void gpio_init_func(uint32_t pin, uint32_t funcsel);

/**
 * @brief Sets output on the selected GPIO pin.
 * @param pin   Integer GPIO pin
 */
void gpio_set(uint32_t pin);

/**
 * @brief Clears output on the selected GPIO pin.
 * @param pin   Integer GPIO pin
 */
void gpio_clr(uint32_t pin);

#endif
