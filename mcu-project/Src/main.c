#include <stdbool.h>
#include <stdint.h>
#include "adc.h"
#include "ff.h"
#include "timer.h"
#include "uart.h"

// Chuong trinh ghi am: TIM3 kich ADC 8 kHz, DMA gom mau thanh khoi, ghi REC_SECONDS giay
// vao file REC.WAV (16 bit mono) tren the SD roi dung. UART 460800 chi in trang thai tung buoc
// Ma ket qua 0 la thanh cong (FR_OK). File REC.WAV cu bi ghi de, khong format the

#define REC_SAMPLE_RATE 8000
#define REC_SECONDS     5
#define REC_SAMPLES     (REC_SAMPLE_RATE * REC_SECONDS)
#define REC_DATA_BYTES  (REC_SAMPLES * 2)

// So mau doi sang 16 bit moi lan ghi: 256 mau = 512 byte = 1 sector
#define PCM_CHUNK_SAMPLES 256

// Tach so thanh cac byte little-endian de dat vao bang khoi tao
#define LE16(v) (uint8_t)((v) & 0xFF), (uint8_t)(((v) >> 8) & 0xFF)
#define LE32(v) LE16(v), LE16((v) >> 16)

// Header WAV PCM 44 byte: do dai du lieu biet truoc nen ghi 1 lan, khong can quay lai sua
static const uint8_t wav_header[44] = {
    'R', 'I', 'F', 'F', LE32(36 + REC_DATA_BYTES), 'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ', LE32(16),      // do dai khoi fmt
    LE16(1),                           // dinh dang 1 = PCM
    LE16(1),                           // so kenh
    LE32(REC_SAMPLE_RATE),
    LE32(REC_SAMPLE_RATE * 2),         // byte moi giay
    LE16(2),                           // byte moi mau
    LE16(16),                          // bit moi mau
    'd', 'a', 't', 'a', LE32(REC_DATA_BYTES)
};

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

static void record(void)
{
    UINT done = 0;

    // Mount, mo file va ghi header truoc khi bat timer: khoi tao the mat hang tram ms,
    // neu ADC da chay thi DMA ghi de mat cac khoi dau
    if (!report("mount", f_mount(&fs, "", 1)))
    {
        return;
    }

    if (!report("open", f_open(&file, "REC.WAV", FA_WRITE | FA_CREATE_ALWAYS)))
    {
        return;
    }

    FRESULT res = f_write(&file, wav_header, sizeof(wav_header), &done);
    uint32_t written = 0;

    timer_start();

    while ((res == FR_OK) && (written < REC_SAMPLES))
    {
        const uint16_t *p_block = adc_wait_block();
        uint32_t count = REC_SAMPLES - written;

        if (count > ADC_BLOCK_SAMPLES)
        {
            count = ADC_BLOCK_SAMPLES;
        }

        // Phai ghi xong ca khoi truoc khi DMA quay lai ghi de (1 khoi thoi gian)
        for (uint32_t offset = 0; (res == FR_OK) && (offset < count); offset += PCM_CHUNK_SAMPLES)
        {
            uint32_t n = count - offset;

            if (n > PCM_CHUNK_SAMPLES)
            {
                n = PCM_CHUNK_SAMPLES;
            }

            // Mau 12 bit khong dau -> 16 bit co dau: tru giua thang (2048) roi nhan 16
            // Muc DC cua mic khong nam dung 2048 nen file con lech DC
            for (uint32_t i = 0; i < n; i++)
            {
                pcm[i] = (int16_t)(((int32_t)p_block[offset + i] - 2048) * 16);
            }

            // Cortex-M3 la little-endian, trung thu tu byte cua WAV nen ghi thang mang int16_t
            res = f_write(&file, pcm, n * sizeof(pcm[0]), &done);

            // The day: f_write van tra FR_OK nhung ghi thieu
            if ((res == FR_OK) && (done != (n * sizeof(pcm[0]))))
            {
                res = FR_DENIED;
            }

            if (res == FR_OK)
            {
                written += n;
            }
        }
    }

    (void)report("write", res);
    (void)report("samples", written);

    // Luon dong file, ke ca khi ghi loi, de giu phan da ghi
    (void)report("close", f_close(&file));
}

int main(void)
{
    // Thu tu bat buoc: timer_init() phat 1 xung TRGO nen phai goi khi trigger ADC con tat,
    // timer_start() (trong record()) chi goi sau khi ADC, DMA va file da san sang
    uart_init(460800);
    timer_init(REC_SAMPLE_RATE);
    adc_init();

    print_str("\r\nrec\r\n");
    record();
    print_str("done\r\n");

    for (;;)
    {
    }
}
