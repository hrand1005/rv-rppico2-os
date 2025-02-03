#ifndef ASM_H
#define ASM_H

#include "types.h"

/**
 * @brief Access memory-mapped register value,
 *        may also be used for assignment
 */
#define AT(addr) ((*(volatile uint32_t *)addr))

/**
 * @brief Triggers breakpoint (ebreak).
 */
void breakpoint();

/**
 * @brief Increments mepc CSR.
 */
void inc_mepc();

/**
 * @brief Sets bits in MIE.
 * @param mask  Integer mask of bits to set
 */
void set_mie(uint32_t mask);

/**
 * @brief Sets bits in MSTATUS.
 * @param mask  Integer mask of bits to set
 */
void set_mstatus(uint32_t mask);

/**
 * @brief Clears bits in MIP.
 * @param mask  Integer mask of bits to clear
 */
void clr_mip(uint32_t);

/**
 * @brief Clears all bits in MEIFA.
 * @param mask  Integer mask of bits to set
 */
void clr_mstatus(uint32_t mask);

/**
 * @brief Clears all bits in MEIFA.
 */
void clr_meifa();

/**
 * @brief Sends event to opposite core.
 */
void sev();

#endif
