#include <stdbool.h>
#include <stdint.h>
#include "storage.h"
#include "ff.h"
#include "recorder.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

// Task storage: chu duy nhat cua the SD (FatFs) va UART log (USART2), nen khong can mutex.
// Nhan lenh qua 1 hang doi va xu ly dung thu tu gui:
//   START: mount neu chua, mo REC.WAV (ghi de), ghi header tam; tra ket qua open cho record
//   DATA:  ghi 1 bo dem PCM roi tra bo dem ve pool
//   STOP:  sua header theo so mau da ghi, dong file, in thong ke
// Uu tien thap hon record: luc driver SD cho the ban, record van chiem CPU de lay khoi DMA
// Phan phat lai sau nay them lenh doc vao cung hang doi nay

#define STORAGE_STACK_WORDS   768
#define STORAGE_TASK_PRIORITY 1
// 3 bo dem x 128 ms: chiu duoc the cham ~384 ms (cong 128 ms cua bo dem DMA)
#define STORAGE_POOL_BUFFERS  3
// Du cho moi bo dem + START + STOP: gui lenh khong bao gio phai cho
#define STORAGE_CMD_QUEUE_LEN (STORAGE_POOL_BUFFERS + 2)

// Tach so thanh cac byte little-endian de dat vao bang khoi tao
#define LE16(v) (uint8_t)((v) & 0xFF), (uint8_t)(((v) >> 8) & 0xFF)
#define LE32(v) LE16(v), LE16((v) >> 16)

typedef enum
{
    STORAGE_CMD_START,
    STORAGE_CMD_DATA,
    STORAGE_CMD_STOP
} storage_cmd_type_t;

typedef struct
{
    storage_cmd_type_t type;
    int16_t *p_pcm;            // DATA: bo dem STORAGE_BUFFER_SAMPLES mau
    storage_stats_t stats;     // STOP
} storage_cmd_t;

static StaticTask_t storage_tcb;
static StackType_t storage_stack[STORAGE_STACK_WORDS];

// Hang doi lenh record -> storage
static StaticQueue_t cmd_queue_ctrl;
static uint8_t cmd_queue_buf[STORAGE_CMD_QUEUE_LEN * sizeof(storage_cmd_t)];
static QueueHandle_t cmd_queue;

// Bo dem trong storage -> record (luu con tro)
static StaticQueue_t free_queue_ctrl;
static uint8_t free_queue_buf[STORAGE_POOL_BUFFERS * sizeof(int16_t *)];
static QueueHandle_t free_queue;

// Ket qua START storage -> record (1 phan tu bool)
static StaticQueue_t reply_queue_ctrl;
static uint8_t reply_queue_buf[sizeof(bool)];
static QueueHandle_t reply_queue;

static int16_t pool[STORAGE_POOL_BUFFERS][STORAGE_BUFFER_SAMPLES];

// Storage ghi, record doc: true khi ghi the loi trong lan ghi hien tai (doc/ghi 1 byte la nguyen tu)
static volatile bool write_failed = false;

// Chi task storage dung
static FATFS fs;
static FIL file;
static bool mounted = false;
static FRESULT write_res = FR_OK;
static uint32_t written = 0;

static void print_str(const char *p_str)
{
    while (*p_str != '\0')
    {
        uart_send_byte(UART_LOG, (uint8_t)*p_str);
        p_str++;
    }
}

// In so thap phan khong dau
static void print_u32(uint32_t value)
{
    char digits[10];
    uint32_t count = 0;

    do
    {
        digits[count] = (char)('0' + (value % 10));
        count++;
        value /= 10;
    } while (value != 0);

    while (count > 0)
    {
        count--;
        uart_send_byte(UART_LOG, (uint8_t)digits[count]);
    }
}

// In "ten: ma", tra ve true neu ma = 0
static bool report(const char *p_name, uint32_t code)
{
    print_str(p_name);
    print_str(": ");
    print_u32(code);
    print_str("\r\n");

    return (code == 0);
}

// Ghi header WAV PCM 44 byte tai vi tri hien tai cua file. Luc mo file chua biet do dai
// nen ghi data_bytes = 0, khi dung ghi quay lai dau file ghi lai voi do dai that
static FRESULT write_header(uint32_t data_bytes)
{
    const uint8_t header[44] = {
        'R', 'I', 'F', 'F', LE32(36 + data_bytes), 'W', 'A', 'V', 'E',
        'f', 'm', 't', ' ', LE32(16),      // do dai khoi fmt
        LE16(1),                           // dinh dang 1 = PCM
        LE16(1),                           // so kenh
        LE32(REC_SAMPLE_RATE),
        LE32(REC_SAMPLE_RATE * 2),         // byte moi giay
        LE16(2),                           // byte moi mau
        LE16(16),                          // bit moi mau
        'd', 'a', 't', 'a', LE32(data_bytes)
    };
    UINT done = 0;
    FRESULT res = f_write(&file, header, sizeof(header), &done);

    // The day: f_write van tra FR_OK nhung ghi thieu
    if ((res == FR_OK) && (done != sizeof(header)))
    {
        res = FR_DENIED;
    }

    return res;
}

static void handle_start(void)
{
    FRESULT res = FR_OK;

    print_str("start\r\n");

    // Mount lan dau, hoac lai sau khi mount/open loi (vd vua cam lai the)
    if (!mounted)
    {
        res = f_mount(&fs, "", 1);
        mounted = report("mount", res);
    }

    if (res == FR_OK)
    {
        res = f_open(&file, "REC.WAV", FA_WRITE | FA_CREATE_ALWAYS);

        if (!report("open", res))
        {
            mounted = false;
        }
    }

    bool opened = (res == FR_OK);

    if (opened)
    {
        written = 0;
        write_res = write_header(0);
        write_failed = (write_res != FR_OK);
    }

    (void)xQueueSend(reply_queue, &opened, portMAX_DELAY);
}

static void handle_data(int16_t *p_pcm)
{
    // Sau lan ghi loi dau tien bo qua cac khoi con lai, chi tra bo dem ve pool
    if (write_res == FR_OK)
    {
        UINT done = 0;

        // Cortex-M3 la little-endian, trung thu tu byte cua WAV nen ghi thang mang int16_t
        write_res = f_write(&file, p_pcm, STORAGE_BUFFER_SAMPLES * sizeof(int16_t), &done);

        if ((write_res == FR_OK) && (done != (STORAGE_BUFFER_SAMPLES * sizeof(int16_t))))
        {
            write_res = FR_DENIED;
        }

        if (write_res == FR_OK)
        {
            written += STORAGE_BUFFER_SAMPLES;
        }
        else
        {
            write_failed = true;
        }
    }

    (void)xQueueSend(free_queue, &p_pcm, portMAX_DELAY);
}

static void handle_stop(const storage_stats_t *p_stats)
{
    (void)report("write", write_res);
    (void)report("samples", written);
    (void)report("overrun", p_stats->overruns);
    (void)report("dropped", p_stats->dropped);

    // Sua do dai trong header theo so mau da ghi, ke ca khi ghi loi, de giu phan da ghi
    FRESULT header_res = f_lseek(&file, 0);

    if (header_res == FR_OK)
    {
        header_res = write_header(written * 2);
    }

    (void)report("header", header_res);
    (void)report("close", f_close(&file));
    print_str("stop\r\n");

    // Word stack con trong it nhat ke tu khi chay: dung de chot kich thuoc stack 2 task
    (void)report("stack_record", p_stats->record_stack_free);
    (void)report("stack_storage", uxTaskGetStackHighWaterMark(NULL));
}

static void storage_task(void *p_arg)
{
    storage_cmd_t cmd;

    (void)p_arg;
    print_str("\r\nrec\r\n");

    for (;;)
    {
        (void)xQueueReceive(cmd_queue, &cmd, portMAX_DELAY);

        switch (cmd.type)
        {
            case STORAGE_CMD_START:
                handle_start();
                break;

            case STORAGE_CMD_DATA:
                handle_data(cmd.p_pcm);
                break;

            case STORAGE_CMD_STOP:
                handle_stop(&cmd.stats);
                break;

            default:
                // Khong xay ra: chi cac ham ben duoi tao lenh
                break;
        }
    }
}

// Tao hang doi, nap pool va tao task storage. Goi trong main() truoc vTaskStartScheduler()
void storage_init(void)
{
    cmd_queue = xQueueCreateStatic(STORAGE_CMD_QUEUE_LEN, sizeof(storage_cmd_t),
                                   cmd_queue_buf, &cmd_queue_ctrl);
    free_queue = xQueueCreateStatic(STORAGE_POOL_BUFFERS, sizeof(int16_t *),
                                    free_queue_buf, &free_queue_ctrl);
    reply_queue = xQueueCreateStatic(1, sizeof(bool), reply_queue_buf, &reply_queue_ctrl);

    for (uint32_t i = 0; i < STORAGE_POOL_BUFFERS; i++)
    {
        int16_t *p_pcm = pool[i];

        (void)xQueueSend(free_queue, &p_pcm, 0);
    }

    TaskHandle_t task = xTaskCreateStatic(storage_task, "storage", STORAGE_STACK_WORDS, NULL,
                                          STORAGE_TASK_PRIORITY, storage_stack, &storage_tcb);

    configASSERT(task != NULL);
}

// Goi tu task record: mo file moi va cho ket qua. true = da mo, duoc bat dau lay mau
bool storage_start_recording(void)
{
    storage_cmd_t cmd = { .type = STORAGE_CMD_START };
    bool opened = false;

    (void)xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
    (void)xQueueReceive(reply_queue, &opened, portMAX_DELAY);

    return opened;
}

// Lay 1 bo dem trong, khong cho. NULL = storage chua ghi kip, pool het
int16_t *storage_get_buffer(void)
{
    int16_t *p_pcm = NULL;

    (void)xQueueReceive(free_queue, &p_pcm, 0);

    return p_pcm;
}

// Gui bo dem da day cho storage ghi; storage tra no ve pool sau khi ghi
void storage_submit_buffer(int16_t *p_pcm)
{
    storage_cmd_t cmd = { .type = STORAGE_CMD_DATA, .p_pcm = p_pcm };

    (void)xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
}

// true neu lan ghi hien tai da gap loi the (record nen dung)
bool storage_write_failed(void)
{
    return write_failed;
}

// Ket thuc lan ghi: storage ghi het cac bo dem da gui truoc, roi sua header va dong file
void storage_stop_recording(const storage_stats_t *p_stats)
{
    storage_cmd_t cmd = { .type = STORAGE_CMD_STOP, .stats = *p_stats };

    (void)xQueueSend(cmd_queue, &cmd, portMAX_DELAY);
}
