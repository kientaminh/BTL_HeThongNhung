#include "adc.h"
#include "board.h"
#include "button.h"
#include "gpio.h"
#include "recorder.h"
#include "timer.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"

#define RECORDER_STACK_WORDS 768u

static StaticTask_t recorder_tcb;
static StackType_t recorder_stack[RECORDER_STACK_WORDS];

static void recorder_task(void *p_arg)
{
    (void)p_arg;
    adc_set_recorder_task(xTaskGetCurrentTaskHandle());

    for (;;)
    {
        // recorder_run() lap mai khi mount thanh cong; mount loi thi thu lai sau 1 s.
        recorder_run();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *p_name)
{
    (void)task;
    (void)p_name;
    __disable_irq();
    for (;;)
    {
    }
}

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

    // Task thu am va ghi the; sau nay co the tao them task phat lai.
    if (xTaskCreateStatic(recorder_task, "record", RECORDER_STACK_WORDS,
                          NULL, 2, recorder_stack, &recorder_tcb) == NULL)
    {
        for (;;)
        {
        }
    }

    vTaskStartScheduler();

    for (;;)
    {
    }
}
