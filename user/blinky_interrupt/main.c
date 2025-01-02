#include "riscv.h"
#include "types.h"

int main() {
    asm volatile("ebreak");

    // let's see what this does
    asm volatile("ecall");
    return 0;
}

void isr_mtimer_irq() {
    breakpoint();
}
