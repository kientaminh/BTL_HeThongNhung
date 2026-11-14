#include <stdint.h>
#include "adc.h"
#include "timer.h"
#include "uart.h"

// Chuong trinh thu mic: TIM3 kich ADC 8 kHz, gui moi mau 1 byte qua UART
// May tinh thu bang Tools/uart_to_wav.py (SAMPLE_RATE phai khop tan so o day)
int main(void)
{
    uart_init(115200);
    adc_init();
    timer_init(8000);

    for (;;)
    {
        // Giu 8 bit cao cua mau 12 bit: 8000 byte/s vua voi 115200 baud (11520 byte/s)
        uart_send_byte((uint8_t)(adc_read() >> 4));
    }
}
