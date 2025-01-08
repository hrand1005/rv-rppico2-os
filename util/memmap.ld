/**
 * Linker script, defines the following:
 *
 *  __text_start
 *  __text_end
 *  __mtext_start
 *  __mtext_end
 *  __utext_start
 *  __utext_end
 *  __image_def_start
 *  __image_def_end
 *  __data_load_start
 *  __data_start
 *  __data_end
 *  __mdata_start
 *  __mdata_end
 *  __udata_start
 *  __udata_end
 *  __rodata_start
 *  __rodata_end
 *  __mrodata_start
 *  __mrodata_end
 *  __urodata_start
 *  __urodata_end
 *  __bss_start
 *  __bss_end
 *  __mbss_start
 *  __mbss_end
 *  __ubss_start
 *  __ubss_end
 *  __mstack0_limit
 *  __mstack0_base
 *  __mstack1_limit
 *  __mstack1_base
 *  __ustack0_limit
 *  __ustack0_base
 *  __ustack1_limit
 *  __ustack1_base
 *
 * TODO:
 *      Use a template version of this linker script to insert the appropriate
 *      binaries for machine and user memory segments, instead of relying on
 *      the following kinds of lines:
 * 
 *      build/kernel*.o(.<section>)
 *      build/user*.o(.<section>)
 *
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

        /* assume text segments <= 4 KB each */
        . = ALIGN(4096);
        __text_start = .;
        __mtext_start = .;
        build/kernel*.o(.text)
        . = ALIGN(4096);
        __mtext_end = .;
        __utext_start = .;
        build/user*.o(.text)
        . = ALIGN(4096);
        __utext_end = .;
        __text_end = .;
    } > FLASH

    .data : ALIGN(4) {
        __data_start = .;
        __mdata_start = .;
        build/kernel*.o(.data*)
        build/kernel*.o(.sdata*)
        build/test*.o(.data*)
        build/test*.o(.sdata*)
        . = ALIGN(4);
        __mdata_end = .;
        __udata_start = .;
        build/user*.o(.data*)
        build/user*.o(.sdata*)
        . = ALIGN(4);
        __udata_end = .;
        __data_end = .;
    } > RAM AT > FLASH
    __data_load_start = LOADADDR(.data);

    .rodata : ALIGN(4) {
        __rodata_start = .;
        __mrodata_start = .;
        build/kernel*.o(.rodata*)
        . = ALIGN(4);
        __mrodata_end = .;
        __urodata_start = .;
        build/user*.o(.rodata*)
        . = ALIGN(4);
        __urodata_end = .;
        __rodata_end = .;
    } > FLASH

    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        __mbss_start = .;
        build/kernel*.o(.bss*)
        build/kernel*.o(.sbss*)
        build/test*.o(.bss*)
        build/test*.o(.sbss*)
        . = ALIGN(4);
        __mbss_end = .;
        __ubss_start = .;
        __gp = .;
        build/user*.o(.bss*)
        build/user*.o(.sbss*)
        . = ALIGN(4);
        __ubss_end = .;
        __bss_end = .;
    } > RAM

    /* core 0 */
    __mstack0_size = 0x2000;
    .mstack0 (NOLOAD) : ALIGN(4096) {
        __mstack0_limit = .;
        . += __mstack0_size;
        __mstack0_base = .;
    } > RAM

    /* core 1 */
    __mstack1_size = 0x2000;
    .mstack1 (NOLOAD) : ALIGN(4096) {
        __mstack1_limit = .;
        . += __mstack1_size;
        __mstack1_base = .;
    } > RAM

    /* core 0 */
    __ustack0_size = 0x2000;
    .ustack0 (NOLOAD) : ALIGN(4096) {
        __ustack0_limit = .;
        . += __ustack0_size;
        __ustack0_base = .;
    } > RAM


    /* core 1 */
    __ustack1_size = 0x2000;
    .ustack1 (NOLOAD) : ALIGN(4096) {
        __ustack1_limit = .;
        . += __ustack1_size;
        __ustack1_base = .;
    } > RAM

}

