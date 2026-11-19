#include "adc.h"
#include "board.h"
#include "button.h"
#include "gpio.h"
#include "recorder.h"
#include "storage.h"
#include "timer.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"

// Loi khong phuc hoi (assert FreeRTOS, tran stack, tao task loi): dung han, bao bang LED
// LED: tat = cho nut, sang deu = dang ghi, nhap nhay = da chet o day
void app_fatal(void)
{
    // Tat ngat truoc: RAM co the da hong, khong de DMA ISR hay SysTick chay tiep
    __disable_irq();

    for (;;)
    {
        // Nhap nhay ~4 Hz bang vong dem CPU vi ngat da tat; tan so chi gan dung
        for (volatile uint32_t i = 0; i < 1000000; i++)
        {
        }

        // Dao PC13 (bit BOARD_LED_PIN cua ODR); PC13 la chan duy nhat dung tren GPIOC
        BOARD_LED_PORT->ODR ^= (1 << BOARD_LED_PIN);
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *p_name)
{
    (void)task;
    (void)p_name;
    app_fatal();
}

int main(void)
{
    // Thu tu bat buoc: timer_init() phat 1 xung TRGO nen phai goi khi trigger ADC con tat,
    // timer_start() (trong task record) chi goi sau khi ADC, DMA va file da san sang
    // Log debug ra PA2/PA3; USART1 (PA9/PA10) de danh cho ESP, chua khoi tao
    uart_init(UART_LOG, 460800);
    timer_init(REC_SAMPLE_RATE);
    adc_init();
    button_init();

    // LED bao dang ghi, sang o muc thap: tat luc khoi dong
    gpio_init(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_MODE_OUTPUT_PUSH_PULL);
    gpio_write(BOARD_LED_PORT, BOARD_LED_PIN, true);

    // Moi module tu tao task cua minh: storage (uu tien 1) giu the SD va UART,
    // record (uu tien 2) lay mau va loc
    storage_init();
    recorder_init();

    vTaskStartScheduler();

    for (;;)
    {
    }
}
