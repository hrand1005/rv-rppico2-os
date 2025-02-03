/**
 * @file fifo.h
 * @brief Defines functions for interacting with interprocessor FIFO queues.
 * @author Herbie Rand
 */

#ifndef FIFO_H
#define FIFO_H

#include "types.h"

#define ST_VLD 0x1
#define ST_RDY 0x2
#define ST_WOF 0x4
#define ST_ROE 0x8

/**
 * @brief Drains core-local interprocessor read FIFO.
 */
void multicore_fifo_drain();

/**
 * @brief Pushes message to core-local interprocessor write FIFO.
 * @param cmd   Integer cmd to opposite core
 */
void multicore_fifo_push_blocking(uint32_t cmd);

/**
 * @brief Waits for a command to arrive on the core-local read FIFO,
 *        then pops and returns the result.
 * @return Integer message from opposite core
 */
uint32_t multicore_fifo_pop_blocking();

#endif
