#include <stdint.h>
#include "adc.h"
#include "timer.h"
#include "uart.h"

// Chuong trinh thu mic: TIM3 kich ADC 8 kHz, DMA gom mau thanh khoi, gui moi mau 1 byte qua UART
// May tinh thu bang Tools/uart_to_wav.py (SAMPLE_RATE phai khop tan so o day)
int main(void)
{
    // Thu tu bat buoc: timer_init() phat 1 xung TRGO nen phai goi khi trigger ADC con tat,
    // timer_start() chi goi sau khi ADC va DMA da san sang
    uart_init(115200);
    timer_init(8000);
    adc_init();
    timer_start();

    for (;;)
    {
        const uint16_t *p_block = adc_wait_block();

        // Giu 8 bit cao cua mau 12 bit: 8000 byte/s vua voi 115200 baud (11520 byte/s)
        // Gui 1 khoi mat 11.1 ms, kip truoc khi DMA ghi de sau 16 ms
        for (uint32_t i = 0; i < ADC_BLOCK_SAMPLES; i++)
        {
            uart_send_byte((uint8_t)(p_block[i] >> 4));
        }
    }
}
