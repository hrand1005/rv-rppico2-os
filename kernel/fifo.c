#include "fifo.h"
#include "asm.h"
#include "rp2350.h"
#include "types.h"

void multicore_fifo_drain() {
    while (AT(SIO_FIFO_ST) & ST_VLD) {
        (void)AT(SIO_FIFO_RD);
    }
}

void multicore_fifo_push_blocking(uint32_t cmd) {
    while (!(AT(SIO_FIFO_ST) & ST_RDY))
        ;
    AT(SIO_FIFO_WR) = cmd;
    sev();
}

uint32_t multicore_fifo_pop_blocking() {
    while (!(AT(SIO_FIFO_ST) & ST_VLD))
        ;
    return AT(SIO_FIFO_RD);
}
