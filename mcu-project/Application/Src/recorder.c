#include <stdbool.h>
#include <stdint.h>
#include "recorder.h"
#include "adc.h"
#include "board.h"
#include "button.h"
#include "ff.h"
#include "fir.h"
#include "gpio.h"
#include "timer.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"

// Chuong trinh ghi am bang nut: giu nut thi ghi, tha nut thi dung. TIM3 kich ADC 8 kHz,
// DMA gom mau thanh khoi, loc FIR roi ghi vao REC.WAV (16 bit mono) tren the SD
// Moi lan nhan nut ghi de REC.WAV cu, khong format the. UART 460800 in trang thai tung buoc,
// ma ket qua 0 la thanh cong (FR_OK)

// Thoi gian 1 khoi DMA (128 ms): khi dang ghi, nut chi duoc doc 1 lan moi khoi
#define REC_BLOCK_MS ((ADC_BLOCK_SAMPLES * 1000) / REC_SAMPLE_RATE)
// Chu ky doc nut khi dang cho
#define REC_IDLE_POLL_MS 5

// So mau doi sang 16 bit moi lan ghi: 256 mau = 512 byte = 1 sector
#define PCM_CHUNK_SAMPLES 256

// Tach so thanh cac byte little-endian de dat vao bang khoi tao
#define LE16(v) (uint8_t)((v) & 0xFF), (uint8_t)(((v) >> 8) & 0xFF)
#define LE32(v) LE16(v), LE16((v) >> 16)

static FATFS fs;
static FIL file;
static int16_t pcm[PCM_CHUNK_SAMPLES];

static void print_str(const char *p_str)
{
    while (*p_str != '\0')
    {
        uart_send_byte((uint8_t)*p_str);
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
        uart_send_byte((uint8_t)digits[count]);
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

// Doi 1 khoi ADC sang 16 bit, loc FIR roi ghi vao file. Cong so mau ghi duoc vao *p_written
// Phai ghi xong ca khoi truoc khi DMA quay lai ghi de (1 khoi thoi gian)
static FRESULT write_block(const uint16_t *p_block, uint32_t *p_written)
{
    FRESULT res = FR_OK;
    UINT done = 0;

    for (uint32_t offset = 0; (res == FR_OK) && (offset < ADC_BLOCK_SAMPLES);
         offset += PCM_CHUNK_SAMPLES)
    {
        // Mau 12 bit khong dau -> 16 bit co dau: tru giua thang (2048) roi nhan 16
        // Muc DC cua mic khong nam dung 2048 nen file con lech DC
        for (uint32_t i = 0; i < PCM_CHUNK_SAMPLES; i++)
        {
            pcm[i] = (int16_t)(((int32_t)p_block[offset + i] - 2048) * 16);
        }

        // Loc thong thap 1 kHz truoc khi ghi; file chi giu ban da loc
        fir_process(pcm, PCM_CHUNK_SAMPLES);

        // Cortex-M3 la little-endian, trung thu tu byte cua WAV nen ghi thang mang int16_t
        res = f_write(&file, pcm, sizeof(pcm), &done);

        if ((res == FR_OK) && (done != sizeof(pcm)))
        {
            res = FR_DENIED;
        }

        if (res == FR_OK)
        {
            *p_written += PCM_CHUNK_SAMPLES;
        }
    }

    return res;
}

// Cho den khi nut (da chong doi) o trang thai pressed
static void wait_button(bool pressed)
{
    do
    {
        vTaskDelay(pdMS_TO_TICKS(REC_IDLE_POLL_MS));
    } while (button_update(REC_IDLE_POLL_MS) != pressed);
}

// Ghi 1 doan vao REC.WAV tu luc goi den khi tha nut (hoac ghi loi)
static void record_once(void)
{
    uint32_t written = 0;

    // Mo file va ghi header truoc khi bat timer: neu ADC da chay thi DMA ghi de mat cac khoi dau
    if (!report("open", f_open(&file, "REC.WAV", FA_WRITE | FA_CREATE_ALWAYS)))
    {
        return;
    }

    FRESULT res = write_header(0);
    uint32_t overruns = adc_get_overrun_count();
    bool held = true;

    fir_reset();
    adc_reset_buffer();
    gpio_write(BOARD_LED_PORT, BOARD_LED_PIN, false);    // LED sang: dang ghi
    timer_start();

    // Ghi tron khoi, kiem tra nut sau moi khoi: dung tre toi da khoang 2 khoi sau khi tha
    while ((res == FR_OK) && held)
    {
        res = write_block(adc_wait_block(), &written);
        held = button_update(REC_BLOCK_MS);
    }

    timer_stop();
    gpio_write(BOARD_LED_PORT, BOARD_LED_PIN, true);
    overruns = adc_get_overrun_count() - overruns;

    (void)report("write", res);
    (void)report("samples", written);
    (void)report("overrun", overruns);

    // Sua do dai trong header theo so mau da ghi, ke ca khi ghi loi, de giu phan da ghi
    FRESULT header_res = f_lseek(&file, 0);

    if (header_res == FR_OK)
    {
        header_res = write_header(written * 2);
    }

    (void)report("header", header_res);
    (void)report("close", f_close(&file));
}

// Mount the roi lap mai: cho nhan nut, ghi den khi tha nut. Chi tra ve khi mount loi
// Goi sau uart_init(), timer_init(), adc_init(), button_init() va khoi tao LED
void recorder_run(void)
{
    print_str("\r\nrec\r\n");

    // Mount truoc khi bat timer: khoi tao the mat hang tram ms
    if (!report("mount", f_mount(&fs, "", 1)))
    {
        return;
    }

    for (;;)
    {
        // Cho tha nut truoc: neu lan ghi truoc dung vi loi trong khi nut con giu,
        // khong duoc bat dau ngay lan ghi moi va ghi de REC.WAV
        wait_button(false);
        wait_button(true);

        print_str("start\r\n");
        record_once();
        print_str("stop\r\n");
    }
}
