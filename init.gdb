target remote localhost:3333
monitor program build/bin/test_uart.elf
monitor reset init
continue

define ic
    set $pc = $pc + 0x4
    continue
end

