#include "led.h"
#include "time.h"

int main() {
    while (1) {
        led_on();
        spin_ms(500);
        led_off();
        spin_ms(500);
    }

    return 0;
}
