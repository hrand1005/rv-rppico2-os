/**
 * @file rp2350.h
 * @brief Provides addresses for memory mapped registers and other
 *        constants for rp2350.
 * @see rp2350 datasheet for details.
 */

#ifndef RP2350_H
#define RP2350_H

#define RVCSR_MEIEA      0xbe0
#define RVCSR_MEIFA      0xbe2
#define RVCSR_MEINEXT    0xbe4
#define RVCSR_MEICONTEXT 0xbe5

#define CLOCKS_BASE             0x40010000
#define CLOCKS_CLK_REF_CTRL     0x40010030
#define CLOCKS_CLK_REF_DIV      0x40010034
#define CLOCKS_CLK_REF_SELECTED 0x40010038
#define CLOCKS_CLK_SYS_CTRL     0x4001003c
#define CLOCKS_CLK_SYS_DIV      0x40010040
#define CLOCKS_CLK_SYS_SELECTED 0x40010044

#define BOOTRAM_BASE 0x400e0000

#define IO_BANK0_BASE 0x40028000

#define PADS_BANK0_BASE 0x40038000

#define SIO_FUNCSEL 0x5

#define SIO_BASE          0xd0000000
#define SIO_GPIO_OUT_SET  0xd0000018
#define SIO_GPIO_OUT_CLR  0xd0000020
#define SIO_GPIO_OE_SET   0xd0000038
#define SIO_GPIO_OE_CLR   0xd0000040
#define SIO_RISCV_SOFTIRQ 0xd00001a0
#define SIO_MTIME_CTRL    0xd00001a4
#define SIO_MTIME         0xd00001b0
#define SIO_MTIMEH        0xd00001b4
#define SIO_MTIMECMP      0xd00001b8
#define SIO_MTIMECMPH     0xd00001bc

#endif
