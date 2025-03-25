/**
 * @file uart.h
 * @brief UART functions, currently uses UART instance 0 only.
 * @author Herbie Rand
 */
#ifndef UART_H
#define UART_H

#include "types.h"

/**
 * @brief Initializes UART0 on the provided GPIO pins.
 *
 * Uses a default baud rate and uart instance 0.
 * Uses TX GPIO pin 0 and RX GPIO pin 1.
 */
void uart_init();

/**
 * @brief Writes a single character to the UART0 transmit buffer.
 * @param c     Byte to transmit
 */
void uart_putc(char c);

/**
 * @brief Gets a single character from teh UART0 receive buffer.
 * @returns Next received byte
 */
char uart_getc();

/**
 * @brief Sets baudrate for UART0 instance.
 * @param baudrate  Integer baudrate
 * @returns Integer baud
 * @see rp2350 datasheet section 12.1.7.1
 */
uint32_t uart_set_baudrate(uint32_t baudrate);

#endif
