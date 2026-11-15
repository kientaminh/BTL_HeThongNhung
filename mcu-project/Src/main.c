#include <stdint.h>
#include "adc.h"
#include "timer.h"
#include "uart.h"
#include "gpio.h"

int main(void)
{
    gpio_init(GPIOC,13,3);
    while(1){
        gpio_write(GPIOC,13,0);
        timer_delay_ms(500);
        gpio_write(GPIOC,13,1);
        timer_delay_ms(500);
    }
}
