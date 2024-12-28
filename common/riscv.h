#ifndef RISCV_H
#define RISCV_H

void breakpoint() {
    asm volatile("ebreak");
}

#endif
