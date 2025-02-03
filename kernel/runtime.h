/**
 * @file runtime.h
 * @brief Defines functions to initialize multicore runtime.
 * @author Herbie Rand
 */

#ifndef RUNTIME_H
#define RUNTIME_H

#include "types.h"

/**
 * @brief Initializes core 1 with the provided vector table address,
 *        stack pointer, and program counter.
 *
 * @param vt    Integer vector table address
 * @param sp    Integer stack pointer
 * @param pc    Integer program counter
 *
 * @see rp2350 datasheet 5.3 "Launching Code on Processor Core 1"
 */
void init_core1(uint32_t vt, uint32_t sp, uint32_t pc);

#endif
