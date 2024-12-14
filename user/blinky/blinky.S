.section .text
.global main
main:
    la a0, 0x400280cc   /* IO_BANK0: GPIO25_CTRL */
    li a1, 0x5
    sw a1, (a0)

    la a0, 0x40038068   /* PADS_BANK0 + GPIO25 */
    lw a1, (a0)
    bclr a1, a1, 8
    sw a1, (a0)

    la a0, 0xd0000038   /* SIO: GPIO_OE_SET */
    bset a1, zero, 25   /* Mask for GPIO pin 25 */
    sw a1, (a0)

    la a0, 0xd0000018   /* SIO: GPIO_OUT_SET */
    la a2, 0xd0000020   /* SIO: GPIO_OUT_CLR */
    
loop:
    sw a1, (a0)

    li a4, 0x200000
    mv a5, zero
spin1:
    addi a5, a5, 1
    blt a5, a4, spin1

    sw a1, (a2)

    li a4, 0x200000
    mv a5, zero
spin2:
    addi a5, a5, 1
    blt a5, a4, spin2

    j loop
