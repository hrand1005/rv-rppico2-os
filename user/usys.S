#include "sys.h"

.global led_on
led_on:
    li a7, SYS_LED_ON
    ecall
    ret

.global led_off
led_off:
    li a7, SYS_LED_OFF
    ecall
    ret

.global spin_ms
spin_ms:
    li a7, SYS_SPIN_MS
    ecall
    ret
