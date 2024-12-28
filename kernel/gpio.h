#ifndef GPIO_H
#define GPIO_H

#include "types.h"

/**
 * @brief Initializes GPIO on the selected GPIO pin.
 * Assumes SIO control function, and enables pin for output.
 */
void gpio_init(uint32_t pin);

/**
 * @brief Sets output on the selected GPIO pin.
 */
void gpio_set(uint32_t pin);

/**
 * @brief Clears output on the selected GPIO pin.
 */
void gpio_clr(uint32_t pin);

#endif
