#include "gpio.h"
#include "asm.h"
#include "rp2350.h"

void gpio_init(uint32_t pin) {
    // select SIO function for the provided GPIO pin
    uint32_t *ctrladdr = (uint32_t *)((IO_BANK0_BASE + 0x4) + (pin * 0x8));
    *ctrladdr = SIO_FUNCSEL;

    // remove pad isolation control with atomic XOR
    uint32_t *padsaddr = (uint32_t *)((PADS_BANK0_BASE + 0x3004) + (pin * 0x4));
    *padsaddr = 0x100;

    // enable output on GPIO pin
    *(uint32_t *)SIO_GPIO_OE_SET = (1 << pin);

    // clear GPIO pin initially
    *(uint32_t *)SIO_GPIO_OUT_CLR = (1 << pin);
}

void gpio_init_func(uint32_t pin, uint32_t funcsel) {
    uint32_t *ctrladdr = (uint32_t *)((IO_BANK0_BASE + 0x4) + (pin * 0x8));
    *ctrladdr = funcsel;

    // remove pad isolation control with atomic XOR
    // uint32_t *padsaddr = (uint32_t *)((PADS_BANK0_BASE + 0x3004) + (pin *
    // 0x4)); *padsaddr = 0x100;

    (void)funcsel;
}

void gpio_set(uint32_t pin) {
    *(uint32_t *)SIO_GPIO_OUT_SET = (1 << pin);
}

void gpio_clr(uint32_t pin) {
    *(uint32_t *)SIO_GPIO_OUT_CLR = (1 << pin);
}

void gpio_set_func(uint32_t pin, uint32_t fn) {
    // TODO: validate inputs
    // Set input enable on, output disable off
    uint32_t val = AT(PADS_BANK0_BASE + 0x4 * pin);
    val &= ~(1 << 7); // clear output disable bit
    val |= (1 << 6);  // set input enable bit
    AT((PADS_BANK0_BASE + 0x4) + (pin * 0x4)) = val;

    // clear fields except for funcsel (at LSBs)
    AT((IO_BANK0_BASE + 0x4) + (pin * 0x8)) = fn;
    // remove pad isolation
    AT((PADS_BANK0_BASE + 0x4) + (pin * 0x4) + ATOMIC_BITCLR_OFFSET) = 0x100;
}
