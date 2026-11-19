#include <stdbool.h>
#include <stdint.h>
#include "recorder.h"
#include "adc.h"
#include "board.h"
#include "button.h"
#include "fir.h"
#include "gpio.h"
#include "storage.h"
#include "timer.h"
#include "FreeRTOS.h"
#include "task.h"

// Task record: giu nut thi ghi, tha nut thi dung. TIM3 kich ADC 8 kHz, DMA gom mau thanh khoi,
// moi khoi doi sang 16 bit, loc FIR vao 1 bo dem cua storage roi gui cho task storage ghi the.
// Task nay so huu nut, LED, ADC/DMA va FIR; khong cham the SD va UART

// Record khong goi FatFs/UART (thuoc storage) nen can it stack; xem "stack_record" tren UART
#define RECORDER_STACK_WORDS   256
// Cao hon storage: luon lay kip khoi DMA ke ca khi storage dang cho the SD
#define RECORDER_TASK_PRIORITY 2

// Thoi gian 1 khoi DMA (128 ms): khi dang ghi, nut chi duoc doc 1 lan moi khoi
#define REC_BLOCK_MS ((ADC_BLOCK_SAMPLES * 1000) / REC_SAMPLE_RATE)
// Chu ky doc nut khi dang cho
#define REC_IDLE_POLL_MS 5

static StaticTask_t recorder_tcb;
static StackType_t recorder_stack[RECORDER_STACK_WORDS];

// Cho den khi nut (da chong doi) o trang thai pressed. Chan on dinh thi ngu han toi ngat
// EXTI cua nut; chi doc nut moi 5 ms trong luc chong doi sau mot canh (~20-25 ms)
// Thong bao den truoc khi ngu van duoc giu lai nen khong lo mat canh
static void wait_button(bool pressed)
{
    while (button_update(REC_IDLE_POLL_MS) != pressed)
    {
        if (button_changing())
        {
            vTaskDelay(pdMS_TO_TICKS(REC_IDLE_POLL_MS));
        }
        else
        {
            (void)ulTaskNotifyTakeIndexed(BUTTON_NOTIFY_INDEX, pdTRUE, portMAX_DELAY);
        }
    }
}

// Doi 1 khoi ADC sang PCM 16 bit co dau vao p_pcm roi loc thong thap 1 kHz tai cho
static void convert_block(const uint16_t *p_block, int16_t *p_pcm)
{
    // Mau 12 bit khong dau -> 16 bit co dau: tru giua thang (2048) roi nhan 16
    // Muc DC cua mic khong nam dung 2048 nen file con lech DC
    for (uint32_t i = 0; i < ADC_BLOCK_SAMPLES; i++)
    {
        p_pcm[i] = (int16_t)(((int32_t)p_block[i] - 2048) * 16);
    }

    fir_process(p_pcm, ADC_BLOCK_SAMPLES);
}

// Ghi 1 doan tu luc goi den khi tha nut (hoac storage bao ghi loi)
static void record_once(void)
{
    storage_stats_t stats = { 0 };
    uint32_t overruns = adc_get_overrun_count();
    bool held = true;

    // Cho storage mo file xong moi bat timer: neu ADC da chay thi cac khoi dau bi mat
    if (!storage_start_recording())
    {
        return;
    }

    fir_reset();
    adc_reset_buffer();
    gpio_write(BOARD_LED_PORT, BOARD_LED_PIN, false);    // LED sang: dang ghi
    timer_start();

    // Dung khi tha nut (tre toi da khoang 2 khoi) hoac khi the ghi loi
    while (held && !storage_write_failed())
    {
        const uint16_t *p_block = adc_wait_block();
        int16_t *p_pcm = storage_get_buffer();

        // Pool het: storage chua ghi kip, bo khoi nay (file thieu 128 ms) thay vi chan lay mau
        if (p_pcm == NULL)
        {
            stats.dropped++;
        }
        else
        {
            convert_block(p_block, p_pcm);
            storage_submit_buffer(p_pcm);
        }

        held = button_update(REC_BLOCK_MS);
    }

    timer_stop();
    gpio_write(BOARD_LED_PORT, BOARD_LED_PIN, true);

    stats.overruns = adc_get_overrun_count() - overruns;
    stats.record_stack_free = uxTaskGetStackHighWaterMark(NULL);
    storage_stop_recording(&stats);
}

// Task record: cho nhan nut, ghi den khi tha nut, lap mai
// Mount/open loi thi bo qua lan nhan nay, lan nhan ke tiep storage thu lai
static void recorder_task(void *p_arg)
{
    (void)p_arg;

    // DMA ISR danh thuc dung task nay khi 1 khoi day (thong bao chi so 0),
    // ngat nut danh thuc khi chan PA1 doi muc (chi so BUTTON_NOTIFY_INDEX)
    adc_set_recorder_task(xTaskGetCurrentTaskHandle());
    button_set_notify_task(xTaskGetCurrentTaskHandle());

    for (;;)
    {
        // Cho tha nut truoc: neu lan ghi truoc dung vi loi trong khi nut con giu,
        // khong duoc bat dau ngay lan ghi moi va ghi de REC.WAV
        wait_button(false);
        wait_button(true);
        record_once();
    }
}

// Tao task record. Goi trong main() sau uart_init(), timer_init(), adc_init(), button_init(),
// khoi tao LED va storage_init(), truoc vTaskStartScheduler()
void recorder_init(void)
{
    TaskHandle_t task = xTaskCreateStatic(recorder_task, "record", RECORDER_STACK_WORDS, NULL,
                                          RECORDER_TASK_PRIORITY, recorder_stack, &recorder_tcb);

    configASSERT(task != NULL);
}
