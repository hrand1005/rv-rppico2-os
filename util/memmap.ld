/**
 * Minimal linker script.
 * Defines the following:
 *
 *  __text_start
 *  __text_end
 *  __image_def_start
 *  __image_def_end
 *  __data_load_start
 *  __data_start
 *  __data_end
 *  __rodata_start
 *  __rodata_end
 *  __bss_start
 *  __bss_end
 *  __mstack0_limit
 *  __mstack0_base
 *  __mstack1_limit
 *  __mstack1_base
 *  __ustack0_limit
 *  __ustack0_base
 *  __ustack1_limit
 *  __ustack1_base
 */

MEMORY
{
    FLASH(rx) : ORIGIN = 0x10000000, LENGTH = 16M
    RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 512k
    SRAM(rx): ORIGIN = 0x20081f00, LENGTH = 252
}

ENTRY(_entry_point)

SECTIONS
{
    .text : {
        /* reset section contains _entry_point */
        KEEP (*(.reset))
        /* mtvec.MODE = Vectored requires 64 byte alignment */
        . = ALIGN(64);
        KEEP (*(.vectors))
        . = ALIGN(4);
        __image_def_start = .;
        KEEP (*(.image_def))
        __image_def_end = .;
        __text_start = .;
        *(.text*)
        __text_end = .;
    } > FLASH

    .data : ALIGN(4) {
        __data_start = .;
        *(.data*)
        *(.sdata*)
        __data_end = .;
    } > RAM AT > FLASH
    __data_load_start = LOADADDR(.data);

    .rodata : ALIGN(4) {
        __rodata_start = .;
        *(.rodata*)
        __rodata_end = .;
    } > FLASH

    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        __bss_end = .;
    } > RAM

    __gp = ALIGN((__bss_start + __bss_end) / 2, 4);

    /* core 0 */
    __mstack0_size = 0x2000;
    .mstack0 (NOLOAD) : ALIGN(4) {
        __mstack0_limit = .;
        . += __mstack0_size;
        __mstack0_base = .;
    } > RAM


    /* core 1 */
    __mstack1_size = 0x2000;
    .mstack1 (NOLOAD) : ALIGN(4) {
        __mstack1_limit = .;
        . += __mstack1_size;
        __mstack1_base = .;
    } > RAM

    /* core 0 */
    __ustack0_size = 0x2000;
    .ustack0 (NOLOAD) : ALIGN(4) {
        __ustack0_limit = .;
        . += __ustack0_size;
        __ustack0_base = .;
    } > RAM


    /* core 1 */
    __ustack1_size = 0x2000;
    .ustack1 (NOLOAD) : ALIGN(4) {
        __ustack1_limit = .;
        . += __ustack1_size;
        __ustack1_base = .;
    } > RAM

}

