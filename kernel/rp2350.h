/**
 * @file rp2350.h
 * @brief Provides addresses for memory mapped registers for rp2350.
 * @see rp2350 datasheet for details.
 */

#define RVCSR_MEIEA      0xbe0
#define RVCSR_MEIFA      0xbe2
#define RVCSR_MEINEXT    0xbe4
#define RVCSR_MEICONTEXT 0xbe5

#define BOOTRAM_BASE 0x400e0000

#define SIO_BASE          0xd0000000
#define SIO_RISCV_SOFTIRQ 0xd00001a0
#define SIO_MTIME_CTRL    0xd00001a4
#define SIO_MTIME         0xd00001b0
#define SIO_MTIMEH        0xd00001b4
#define SIO_MTIMECMP      0xd00001b8
#define SIO_MTIMECMPH     0xd00001bc
