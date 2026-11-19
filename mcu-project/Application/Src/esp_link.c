#include <stdint.h>
#include "esp_link.h"
#include "crc16.h"
#include "recorder.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

// Task comm: chu duy nhat cua UART ESP (USART1 PA9/PA10). Nhan khung PCM tu storage, gui sang
// ESP32-S3 theo dinh dang esp32/README.md, roi tra bo dem lai. Khong goi FatFs, khong in log.
// Khung (8N1, 460800): A5 5A C3 3C | 2048 byte PCM int16 little-endian | CRC-16 (byte thap truoc)
// ESP khong tra ACK va phat ngay ra loa (I2S 8 kHz), bo dem nhan UART chi 8 KB (~4 khung):
// gui nhanh hon toc do phat se tran va mat khung, nen task nay gui dung nhip 128 ms/khung

#define ESP_LINK_BAUD          460800    // phai khop UART_BAUD trong esp32/include/config.h
#define ESP_LINK_STACK_WORDS   256
// Bang storage: chia luot CPU theo tick khi ca hai cung ban, khong lan task record (uu tien 2)
#define ESP_LINK_TASK_PRIORITY 1

// Thoi gian phat 1 khung (128 ms o 8 kHz)
#define ESP_LINK_FRAME_MS      ((ESP_LINK_FRAME_SAMPLES * 1000) / REC_SAMPLE_RATE)
// Cho phep gui truoc toi da 2 khung so voi nhip phat: du lap khoang trong I2S khi UART
// truyen 1 khung (~45 ms), van nam trong bo dem 8 KB cua ESP
#define ESP_LINK_LEAD_MS       (2 * ESP_LINK_FRAME_MS)

static const uint8_t frame_sync[4] = { 0xA5, 0x5A, 0xC3, 0x3C };

static StaticTask_t esp_link_tcb;
static StackType_t esp_link_stack[ESP_LINK_STACK_WORDS];

// Khung cho gui: storage -> comm
static StaticQueue_t submit_queue_ctrl;
static uint8_t submit_queue_buf[ESP_LINK_MAX_IN_FLIGHT * sizeof(int16_t *)];
static QueueHandle_t submit_queue;

// Khung da gui xong: comm -> storage
static StaticQueue_t sent_queue_ctrl;
static uint8_t sent_queue_buf[ESP_LINK_MAX_IN_FLIGHT * sizeof(int16_t *)];
static QueueHandle_t sent_queue;

// Gui 1 khung day du: sync, PCM, CRC (ham chan ~45 ms o 460800 baud)
static void send_frame(const int16_t *p_pcm)
{
    const uint8_t *p_bytes = (const uint8_t *)p_pcm;
    const uint32_t len = ESP_LINK_FRAME_SAMPLES * sizeof(int16_t);
    uint16_t crc = crc16_update(CRC16_INIT, p_bytes, len);

    uart_send_buffer(UART_ESP, frame_sync, sizeof(frame_sync));
    // Cortex-M3 little-endian: mang int16_t trong RAM da dung thu tu byte ESP can
    uart_send_buffer(UART_ESP, p_bytes, len);
    uart_send_byte(UART_ESP, (uint8_t)(crc & 0xFF));
    uart_send_byte(UART_ESP, (uint8_t)(crc >> 8));
}

static void esp_link_task(void *p_arg)
{
    const TickType_t frame_ticks = pdMS_TO_TICKS(ESP_LINK_FRAME_MS);
    const TickType_t lead_ticks = pdMS_TO_TICKS(ESP_LINK_LEAD_MS);
    // Thoi diem ESP phat het cac khung da gui (uoc tinh)
    TickType_t play_end = 0;
    int16_t *p_pcm = NULL;

    (void)p_arg;

    for (;;)
    {
        (void)xQueueReceive(submit_queue, &p_pcm, portMAX_DELAY);

        TickType_t now = xTaskGetTickCount();

        // ESP da phat het (luong moi hoac bi tre): tinh nhip lai tu bay gio
        if ((TickType_t)(now - play_end) < (portMAX_DELAY / 2))
        {
            play_end = now;
        }

        // Dang di truoc qua lead: cho cho ESP phat bot
        TickType_t ahead = play_end - now;

        if (ahead > lead_ticks)
        {
            vTaskDelay(ahead - lead_ticks);
        }

        send_frame(p_pcm);
        play_end += frame_ticks;

        (void)xQueueSend(sent_queue, &p_pcm, portMAX_DELAY);
    }
}

// Cau hinh USART1 va tao task comm. Goi trong main() truoc vTaskStartScheduler()
void esp_link_init(void)
{
    uart_init(UART_ESP, ESP_LINK_BAUD);

    submit_queue = xQueueCreateStatic(ESP_LINK_MAX_IN_FLIGHT, sizeof(int16_t *),
                                      submit_queue_buf, &submit_queue_ctrl);
    sent_queue = xQueueCreateStatic(ESP_LINK_MAX_IN_FLIGHT, sizeof(int16_t *),
                                    sent_queue_buf, &sent_queue_ctrl);

    TaskHandle_t task = xTaskCreateStatic(esp_link_task, "esp", ESP_LINK_STACK_WORDS, NULL,
                                          ESP_LINK_TASK_PRIORITY, esp_link_stack, &esp_link_tcb);

    configASSERT(task != NULL);
}

// Dua 1 khung ESP_LINK_FRAME_SAMPLES mau cho comm gui. p_pcm thuoc nguoi goi, phai giu nguyen
// den khi lay lai bang esp_link_take_sent(). Giu toi da ESP_LINK_MAX_IN_FLIGHT khung cung luc
void esp_link_submit(int16_t *p_pcm)
{
    (void)xQueueSend(submit_queue, &p_pcm, portMAX_DELAY);
}

// Cho 1 khung gui xong (thu tu FIFO), tra con tro de nguoi goi dung lai
int16_t *esp_link_take_sent(void)
{
    int16_t *p_pcm = NULL;

    (void)xQueueReceive(sent_queue, &p_pcm, portMAX_DELAY);

    return p_pcm;
}
