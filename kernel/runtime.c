/**
 * @file runtime.c
 * @brief Implements functions to initialize multicore runtime.
 * @author Herbie Rand
 */

#include "runtime.h"
#include "asm.h"
#include "fifo.h"

void init_core1(uint32_t vt, uint32_t sp, uint32_t pc) {
    uint32_t cmd;
    uint32_t resp;
    uint32_t cmd_sequence[6];

    cmd_sequence[0] = 0;
    cmd_sequence[1] = 0;
    cmd_sequence[2] = 1;
    cmd_sequence[3] = vt | 0x1; // enable vectored mode
    cmd_sequence[4] = sp;
    cmd_sequence[5] = pc;

    uint32_t i = 0;
    do {
        cmd = cmd_sequence[i];
        if (!cmd) {
            multicore_fifo_drain();
            sev();
        }
        multicore_fifo_push_blocking(cmd);
        resp = multicore_fifo_pop_blocking();
        i = (cmd == resp) ? (i + 1) : 0;
    } while (i < 6);
}
