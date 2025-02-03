#ifndef ASM_H
#define ASM_H

#include "types.h"

void breakpoint();

void inc_mepc();

void set_mie(uint32_t);

void set_mstatus(uint32_t);

void clr_mip(uint32_t);

void clr_meifa();

void clr_mstatus();

#endif
