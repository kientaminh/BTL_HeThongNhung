#include "adc.h"
#include "board.h"
#include "button.h"
#include "gpio.h"
#include "recorder.h"
#include "timer.h"
#include "uart.h"

int main(void)
{
    // Thu tu bat buoc: timer_init() phat 1 xung TRGO nen phai goi khi trigger ADC con tat,
    // timer_start() (trong recorder_run()) chi goi sau khi ADC, DMA va file da san sang
    uart_init(460800);
    timer_init(REC_SAMPLE_RATE);
    adc_init();
    button_init();

    // LED bao dang ghi, sang o muc thap: tat luc khoi dong
    gpio_init(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_MODE_OUTPUT_PUSH_PULL);
    gpio_write(BOARD_LED_PORT, BOARD_LED_PIN, true);

    // Giu nut thi ghi, tha nut thi dung; chi tra ve khi mount the loi
    recorder_run();

    for (;;)
    {
    }
}
