#include <stdbool.h>
#include <stdint.h>
#include "ff.h"
#include "sd_card.h"
#include "uart.h"

// Chuong trinh thu the SD (tam thay vong thu mic): in ket qua tung buoc qua UART 115200
// Ma ket qua 0 la thanh cong (SD_CARD_OK, FR_OK), dung o buoc loi dau tien
// Chi ghi qua FatFs (file TEST.TXT), khong ghi khoi tho va khong format the

static const char test_text[] = "sd card ok na 123\r\n";

static FATFS fs;
static FIL file;
static uint8_t buf[SD_CARD_BLOCK_SIZE];

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

static void sd_test(void)
{
    uint32_t sector_count = 0;
    UINT done = 0;

    // Buoc 1: day noi, SPI va trinh tu khoi tao the
    if (!report("init", sd_card_init()))
    {
        return;
    }

    // Buoc 2: so sector 512 byte, so voi dung luong ghi tren the
    if (!report("sector count", sd_card_get_sector_count(&sector_count)))
    {
        return;
    }
    (void)report("sectors", sector_count);

    // Buoc 3: doc sector 0, the da format co chu ky 85 170 (0x55 0xAA) o 2 byte cuoi
    if (!report("read sector 0", sd_card_read_blocks(buf, 0, 1)))
    {
        return;
    }
    (void)report("byte 510", buf[510]);
    (void)report("byte 511", buf[511]);

    // Buoc 4: ghi file qua FatFs roi doc lai de so
    if (!report("mount", f_mount(&fs, "", 1)))
    {
        return;
    }

    if (!report("open write", f_open(&file, "TEST.TXT", FA_WRITE | FA_CREATE_ALWAYS)))
    {
        return;
    }
    (void)report("write", f_write(&file, test_text, sizeof(test_text) - 1, &done));
    (void)report("written", done);
    if (!report("close", f_close(&file)))
    {
        return;
    }

    if (!report("open read", f_open(&file, "TEST.TXT", FA_READ)))
    {
        return;
    }
    done = 0;
    (void)report("read", f_read(&file, buf, sizeof(buf), &done));
    (void)f_close(&file);

    // So do dai va noi dung doc lai voi test_text: 0 la khop
    uint32_t mismatch = (done == (sizeof(test_text) - 1)) ? 0 : 1;

    for (uint32_t i = 0; (mismatch == 0) && (i < done); i++)
    {
        if (buf[i] != (uint8_t)test_text[i])
        {
            mismatch = 1;
        }
    }
    (void)report("compare", mismatch);
}

int main(void)
{
    uart_init(115200);

    print_str("\r\nsd test\r\n");
    sd_test();
    print_str("done\r\n");

    for (;;)
    {
    }
}
