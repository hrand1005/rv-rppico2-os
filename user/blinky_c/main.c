/**
 * @file main.c
 * @brief Blinky implementation in C, plus some
 *        potentially informative comments.
 * @author Herbie Rand
 */
#define SIO_FUNCSEL (0x5)
#define LED_MASK    (1 << 25)

#define IO_BANK0_BASE        (0x40028000)
#define IO_BANK0_GPIO25_CTRL (IO_BANK0_BASE + 0xcc)

#define PADS_BANK0_BASE   (0x40038000)
#define PADS_BANK0_GPIO25 (PADS_BANK0_BASE + 0x68)

#define SIO_BASE         (0xd0000000)
#define SIO_GPIO_OUT_SET (SIO_BASE + 0x18)
#define SIO_GPIO_OUT_CLR (SIO_BASE + 0x20)
#define SIO_GPIO_OE_SET  (SIO_BASE + 0x38)
#define SIO_GPIO_OE_CLR  (SIO_BASE + 0x40)

typedef unsigned int uint32_t;

int main() {
    // clear gpio output values
    // *(uint32_t *)(SIO_GPIO_OE_CLR) = LED_MASK;
    // *(uint32_t *)(SIO_GPIO_OUT_CLR) = LED_MASK;

    // choose SIO function for GPIO pin 25
    *(uint32_t *)(IO_BANK0_GPIO25_CTRL) = SIO_FUNCSEL;

    // atomic XOR bit 8 to remove pad isolation control
    // *(uint32_t *)(PADS_BANK0_GPIO25 + 0x3000) = 0x100;
    // uint32_t gpio25_status = *(uint32_t *)(PADS_BANK0_GPIO25);

    *(uint32_t *)(PADS_BANK0_GPIO25) &= ~0x100;
    uint32_t gpio25_status = *(uint32_t *)(PADS_BANK0_GPIO25);

    // enable output on GPIO pin 25 (our LED)
    *(uint32_t *)(SIO_GPIO_OE_SET) = LED_MASK;

    while (1) {
        // set the LED signal and then spin for a while
        *(uint32_t *)(SIO_GPIO_OUT_SET) = LED_MASK;
        for (int i = 0; i < 400000; i++)
            ;

        // clear the LED signal and then spin for a while
        *(uint32_t *)(SIO_GPIO_OUT_CLR) = LED_MASK;
        for (int i = 0; i < 400000; i++)
            ;
    }
    return -1;
}
