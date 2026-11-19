#include "sd_card.h"

#include "board.h"

// Toc do SPI = PCLK2 (72 MHz) / he so chia, chon bang BR (bit 5:3 cua CR1)
// Khoi tao the: BR = 111 (chia 256 -> 281 kHz, chuan SD yeu cau 100..400 kHz)
// Truyen du lieu: BR = 011 (chia 16 -> 4.5 MHz), du cham cho day noi roi va module co dien tro noi tiep
#define SD_SPI_BR_MASK          (7 << 3)
#define SD_SPI_BR_INIT          (7 << 3)
#define SD_SPI_BR_DATA          (3 << 3)

// Cac gioi han cho la so vong lap, khong phai don vi thoi gian
#define SD_SPI_TIMEOUT          65535
#define SD_READY_TIMEOUT        250000
// Moi lan thu ACMD41 gui khoang 21 byte o 281 kHz = 0.6 ms: 4000 lan = 2.4 s
// The duoc phep mat toi 1 s de roi trang thai idle
#define SD_INIT_RETRIES         1000
#define SD_TOKEN_TIMEOUT        250000
#define SD_DATA_TOKEN           0xFE
#define SD_DATA_ACCEPTED        0x05
#define SD_R1_IDLE              0x01
#define SD_R1_ILLEGAL_COMMAND   0x04
#define SD_CMD0                 0
#define SD_CMD8                 8
#define SD_CMD9                 9
#define SD_CMD16                16
#define SD_CMD17                17
#define SD_CMD24                24
#define SD_CMD55                55
#define SD_CMD58                58
#define SD_ACMD41               41

static bool s_is_initialized;
static bool s_is_high_capacity;

static void sd_card_select(void);
static void sd_card_deselect(void);
static bool sd_card_wait_ready(uint32_t timeout);
static bool sd_card_read_data(uint8_t *p_buffer, uint32_t length);
static uint8_t sd_card_send_command(uint8_t command, uint32_t argument);
static sd_card_result_t sd_card_read_csd(uint8_t *p_csd);
static sd_card_result_t sd_card_read_block(uint8_t *p_buffer,
                                           uint32_t sector);
static sd_card_result_t sd_card_write_block(const uint8_t *p_buffer,
                                            uint32_t sector);

// Gui 1 byte va nhan 1 byte qua SPI, false neu het thoi gian cho
static bool sd_card_spi_transfer(uint8_t tx_data, uint8_t *p_rx_data)
{
    uint32_t timeout = SD_SPI_TIMEOUT;

    // Cho TXE = 1 (bit 1 cua SR): bo dem phat trong
    while (((BOARD_SD_SPI->SR & (1 << 1)) == 0) && (timeout > 0))
    {
        timeout--;
    }

    if (timeout == 0)
    {
        return false;
    }

    // Truy cap DR theo 8 bit vi SPI cau hinh khung du lieu 8 bit
    *(__IO uint8_t *)&BOARD_SD_SPI->DR = tx_data;
    timeout = SD_SPI_TIMEOUT;

    // Cho RXNE = 1 (bit 0 cua SR): da nhan xong 1 byte
    while (((BOARD_SD_SPI->SR & (1 << 0)) == 0) && (timeout > 0))
    {
        timeout--;
    }

    if (timeout == 0)
    {
        return false;
    }

    *p_rx_data = *(__IO uint8_t *)&BOARD_SD_SPI->DR;
    return true;
}

// Nhan 1 byte: gui 0xFF de giu MOSI o muc cao trong luc doc
static bool sd_card_spi_receive(uint8_t *p_rx_data)
{
    return sd_card_spi_transfer(0xFF, p_rx_data);
}

// Doi toc do SPI: baud_rate la gia tri BR da dich (SD_SPI_BR_INIT, SD_SPI_BR_DATA)
static void sd_card_spi_set_speed(uint32_t baud_rate)
{
    // Tat SPI (bit 6: SPE) truoc khi doi BR, roi bat lai
    BOARD_SD_SPI->CR1 &= ~(1 << 6);
    BOARD_SD_SPI->CR1 = (BOARD_SD_SPI->CR1 & ~SD_SPI_BR_MASK) | baud_rate;
    BOARD_SD_SPI->CR1 |= (1 << 6);
}

// Khoi tao chan va SPI1 o che do master, mode 0 (CPOL = 0, CPHA = 0), 8 bit, MSB truoc
static void sd_card_spi_init(void)
{
    // Bat clock GPIOA (bit 2: IOPAEN) va SPI1 (bit 12: SPI1EN)
    // Di cung BOARD_SD_SPI, BOARD_SD_PORT trong board.h
    RCC->APB2ENR |= (1 << 2) | (1 << 12);

    // Chan 0..7 nam trong CRL, moi chan 4 bit
    // CS: output push-pull 50 MHz (MODE = 11, CNF = 00 -> 0x3)
    // SCK, MOSI: alternate function push-pull 50 MHz (MODE = 11, CNF = 10 -> 0xB)
    // MISO: input floating (MODE = 00, CNF = 01 -> 0x4)
    uint32_t cs_shift = BOARD_SD_CS_PIN * 4;
    uint32_t sck_shift = BOARD_SD_SCK_PIN * 4;
    uint32_t miso_shift = BOARD_SD_MISO_PIN * 4;
    uint32_t mosi_shift = BOARD_SD_MOSI_PIN * 4;
    uint32_t mask = 0xF;         // 4 bit cau hinh cua chan
    uint32_t out_pp = 0x3;       // MODE = 11, CNF = 00
    uint32_t af_pp = 0xB;        // MODE = 11, CNF = 10
    uint32_t in_floating = 0x4;  // MODE = 00, CNF = 01
    uint32_t pin_mask = (mask << cs_shift) | (mask << sck_shift)
                      | (mask << miso_shift) | (mask << mosi_shift);

    BOARD_SD_PORT->CRL &= ~pin_mask;
    BOARD_SD_PORT->CRL |= (out_pp << cs_shift) | (af_pp << sck_shift)
                        | (in_floating << miso_shift) | (af_pp << mosi_shift);

    // Keo CS len cao (bit pin cua BSRR): chua chon the
    BOARD_SD_PORT->BSRR = (1 << BOARD_SD_CS_PIN);

    // Master (bit 2: MSTR), quan ly NSS bang phan mem (bit 9: SSM, bit 8: SSI = 1)
    // Toc do khoi tao: BR = 111 (bit 5:3)
    BOARD_SD_SPI->CR1 = (1 << 2) | (1 << 9) | (1 << 8) | SD_SPI_BR_INIT;
    BOARD_SD_SPI->CR2 = 0;

    // Bat SPI (bit 6: SPE)
    BOARD_SD_SPI->CR1 |= (1 << 6);
}

// Chon the: keo CS xuong thap (BRR) roi gui 1 byte dem
static void sd_card_select(void)
{
    uint8_t unused;

    BOARD_SD_PORT->BRR = (1 << BOARD_SD_CS_PIN);
    (void)sd_card_spi_receive(&unused);
}

// Bo chon the: keo CS len cao (BSRR) roi gui 1 byte dem de the nha MISO
static void sd_card_deselect(void)
{
    uint8_t unused;

    BOARD_SD_PORT->BSRR = (1 << BOARD_SD_CS_PIN);
    (void)sd_card_spi_receive(&unused);
}

// Cho the het ban (the tra 0xFF khi san sang), timeout la so byte toi da doc thu
static bool sd_card_wait_ready(uint32_t timeout)
{
    uint8_t response = 0;

    do
    {
        if (!sd_card_spi_receive(&response))
        {
            return false;
        }
        timeout--;
    } while ((response != 0xFF) && (timeout > 0));

    return (response == 0xFF);
}

// Gui lenh 6 byte va tra ve phan hoi R1 (0xFF neu loi SPI hoac the khong tra loi)
// The van duoc chon khi ham tra ve: ben goi phai sd_card_deselect()
static uint8_t sd_card_send_command(uint8_t command, uint32_t argument)
{
    uint8_t response = 0xFF;
    uint8_t unused;
    uint8_t crc = 0x01;

    sd_card_deselect();
    sd_card_select();

    // O che do SPI chi CMD0 va CMD8 bi kiem tra CRC, cac lenh khac dung CRC gia
    if (command == SD_CMD0)
    {
        crc = 0x95;
    }
    else if (command == SD_CMD8)
    {
        crc = 0x87;
    }

    if (!sd_card_spi_transfer((uint8_t)(0x40 | command), &unused)
        || !sd_card_spi_transfer((uint8_t)(argument >> 24), &unused)
        || !sd_card_spi_transfer((uint8_t)(argument >> 16), &unused)
        || !sd_card_spi_transfer((uint8_t)(argument >> 8), &unused)
        || !sd_card_spi_transfer((uint8_t)argument, &unused)
        || !sd_card_spi_transfer(crc, &unused))
    {
        return 0xFF;
    }

    // R1 hop le co bit 7 = 0, the tra loi trong toi da 8 byte
    for (uint32_t attempt = 0; attempt < 10; attempt++)
    {
        if (!sd_card_spi_receive(&response))
        {
            return 0xFF;
        }

        if ((response & 0x80) == 0)
        {
            break;
        }
    }

    return response;
}

// Cho token du lieu (0xFE), doc length byte roi bo qua 2 byte CRC
static bool sd_card_read_data(uint8_t *p_buffer, uint32_t length)
{
    uint8_t token = 0xFF;
    uint8_t crc;
    uint32_t timeout = SD_TOKEN_TIMEOUT;

    do
    {
        if (!sd_card_spi_receive(&token))
        {
            return false;
        }
        timeout--;
    } while ((token == 0xFF) && (timeout > 0));

    if ((token != SD_DATA_TOKEN) || (timeout == 0))
    {
        return false;
    }

    for (uint32_t index = 0; index < length; index++)
    {
        if (!sd_card_spi_receive(&p_buffer[index]))
        {
            return false;
        }
    }

    return sd_card_spi_receive(&crc) && sd_card_spi_receive(&crc);
}

// Doc thanh ghi CSD 16 byte (CMD9)
static sd_card_result_t sd_card_read_csd(uint8_t *p_csd)
{
    uint8_t response;

    response = sd_card_send_command(SD_CMD9, 0);
    if (response != 0)
    {
        sd_card_deselect();
        return SD_CARD_ERROR;
    }

    if (!sd_card_read_data(p_csd, 16))
    {
        sd_card_deselect();
        return SD_CARD_TIMEOUT;
    }

    sd_card_deselect();
    return SD_CARD_OK;
}

// The SDHC danh dia chi theo khoi, the dung luong thuong danh dia chi theo byte
static uint32_t sd_card_sector_argument(uint32_t sector)
{
    if (s_is_high_capacity)
    {
        return sector;
    }

    return sector * SD_CARD_BLOCK_SIZE;
}

// Doc 1 khoi 512 byte (CMD17)
static sd_card_result_t sd_card_read_block(uint8_t *p_buffer, uint32_t sector)
{
    uint8_t response;

    response = sd_card_send_command(SD_CMD17,
                                    sd_card_sector_argument(sector));
    if (response != 0)
    {
        sd_card_deselect();
        return SD_CARD_ERROR;
    }

    if (!sd_card_read_data(p_buffer, SD_CARD_BLOCK_SIZE))
    {
        sd_card_deselect();
        return SD_CARD_TIMEOUT;
    }

    sd_card_deselect();
    return SD_CARD_OK;
}

// Ghi 1 khoi 512 byte (CMD24) va cho the ghi xong
static sd_card_result_t sd_card_write_block(const uint8_t *p_buffer, uint32_t sector)
{
    uint8_t response;
    uint8_t unused;

    response = sd_card_send_command(SD_CMD24,
                                    sd_card_sector_argument(sector));
    if (response != 0)
    {
        sd_card_deselect();
        return SD_CARD_ERROR;
    }

    if (!sd_card_spi_transfer(SD_DATA_TOKEN, &unused))
    {
        sd_card_deselect();
        return SD_CARD_TIMEOUT;
    }

    for (uint32_t index = 0; index < SD_CARD_BLOCK_SIZE; index++)
    {
        if (!sd_card_spi_transfer(p_buffer[index], &unused))
        {
            sd_card_deselect();
            return SD_CARD_TIMEOUT;
        }
    }

    // 2 byte CRC gia, sau do the tra ve data response
    if (!sd_card_spi_transfer(0xFF, &unused)
        || !sd_card_spi_transfer(0xFF, &unused)
        || !sd_card_spi_receive(&response))
    {
        sd_card_deselect();
        return SD_CARD_TIMEOUT;
    }

    if ((response & 0x1F) != SD_DATA_ACCEPTED)
    {
        sd_card_deselect();
        return SD_CARD_ERROR;
    }

    if (!sd_card_wait_ready(SD_READY_TIMEOUT))
    {
        sd_card_deselect();
        return SD_CARD_TIMEOUT;
    }

    sd_card_deselect();
    return SD_CARD_OK;
}

// Khoi tao SPI va the (SD v1, v2, SDHC) theo trinh tu che do SPI
sd_card_result_t sd_card_init(void)
{
    uint8_t response;
    uint8_t unused;
    uint8_t ocr[4];
    bool is_v2 = false;

    s_is_initialized = false;
    s_is_high_capacity = false;
    sd_card_spi_init();
    sd_card_spi_set_speed(SD_SPI_BR_INIT);
    sd_card_deselect();

    // It nhat 74 xung clock khi CS cao de the vao che do SPI
    for (uint32_t index = 0; index < 10; index++)
    {
        if (!sd_card_spi_receive(&unused))
        {
            return SD_CARD_TIMEOUT;
        }
    }

    // CMD0: reset the ve trang thai idle
    response = 0xFF;
    for (uint32_t attempt = 0; attempt < 10; attempt++)
    {
        response = sd_card_send_command(SD_CMD0, 0);
        sd_card_deselect();
        if (response == SD_R1_IDLE)
        {
            break;
        }
    }

    if (response != SD_R1_IDLE)
    {
        return SD_CARD_NOT_READY;
    }

    // CMD8: the v2 tra lai dien ap (0x01) va mau kiem tra (0xAA), the v1 bao lenh sai
    response = sd_card_send_command(SD_CMD8, 0x000001AA);
    if (response == SD_R1_IDLE)
    {
        for (uint32_t index = 0; index < 4; index++)
        {
            if (!sd_card_spi_receive(&ocr[index]))
            {
                sd_card_deselect();
                return SD_CARD_TIMEOUT;
            }
        }
        sd_card_deselect();

        if ((ocr[2] != 0x01) || (ocr[3] != 0xAA))
        {
            return SD_CARD_ERROR;
        }
        is_v2 = true;
    }
    else if (response == (SD_R1_IDLE | SD_R1_ILLEGAL_COMMAND))
    {
        sd_card_deselect();
    }
    else
    {
        sd_card_deselect();
        return SD_CARD_ERROR;
    }

    // ACMD41 (CMD55 roi CMD41) lap lai den khi the roi idle, bit 30 (HCS) bao ho tro SDHC
    response = SD_R1_IDLE;
    for (uint32_t attempt = 0; attempt < SD_INIT_RETRIES; attempt++)
    {
        response = sd_card_send_command(SD_CMD55, 0);
        if ((response != SD_R1_IDLE) && (response != 0))
        {
            sd_card_deselect();
            return SD_CARD_ERROR;
        }

        response = sd_card_send_command(SD_ACMD41,
                                        is_v2 ? 0x40000000 : 0);
        sd_card_deselect();
        if (response == 0)
        {
            break;
        }
        if (response != SD_R1_IDLE)
        {
            return SD_CARD_ERROR;
        }
    }

    if (response != 0)
    {
        return SD_CARD_TIMEOUT;
    }

    // CMD58: doc OCR, bit CCS (bit 6 cua byte dau) = 1 la the SDHC
    if (is_v2)
    {
        response = sd_card_send_command(SD_CMD58, 0);
        if (response != 0)
        {
            sd_card_deselect();
            return SD_CARD_ERROR;
        }

        for (uint32_t index = 0; index < 4; index++)
        {
            if (!sd_card_spi_receive(&ocr[index]))
            {
                sd_card_deselect();
                return SD_CARD_TIMEOUT;
            }
        }
        sd_card_deselect();

        s_is_high_capacity = ((ocr[0] & 0x40) != 0);
    }

    // CMD16: dat kich thuoc khoi 512 byte cho the dung luong thuong
    if (!s_is_high_capacity)
    {
        response = sd_card_send_command(SD_CMD16, SD_CARD_BLOCK_SIZE);
        sd_card_deselect();
        if (response != 0)
        {
            return SD_CARD_ERROR;
        }
    }

    sd_card_spi_set_speed(SD_SPI_BR_DATA);
    s_is_initialized = true;
    return SD_CARD_OK;
}

// true neu sd_card_init() da thanh cong
bool sd_card_is_initialized(void)
{
    return s_is_initialized;
}

// Doc sector_count khoi lien tiep tu first_sector vao p_buffer
sd_card_result_t sd_card_read_blocks(uint8_t *p_buffer,
                                     uint32_t first_sector,
                                     uint32_t sector_count)
{
    if ((p_buffer == 0) || (sector_count == 0))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }
    if (!s_is_initialized)
    {
        return SD_CARD_NOT_READY;
    }
    // Chan tran so sector, va tran dia chi byte voi the dung luong thuong
    if ((sector_count - 1) > (UINT32_MAX - first_sector))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }
    if ((!s_is_high_capacity)
        && ((first_sector + sector_count - 1)
            > (UINT32_MAX / SD_CARD_BLOCK_SIZE)))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }

    for (uint32_t index = 0; index < sector_count; index++)
    {
        sd_card_result_t result = sd_card_read_block(
            &p_buffer[index * SD_CARD_BLOCK_SIZE], first_sector + index);
        if (result != SD_CARD_OK)
        {
            return result;
        }
    }

    return SD_CARD_OK;
}

// Ghi sector_count khoi lien tiep tu first_sector tu p_buffer
sd_card_result_t sd_card_write_blocks(const uint8_t *p_buffer,
                                      uint32_t first_sector,
                                      uint32_t sector_count)
{
    if ((p_buffer == 0) || (sector_count == 0))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }
    if (!s_is_initialized)
    {
        return SD_CARD_NOT_READY;
    }
    // Chan tran so sector, va tran dia chi byte voi the dung luong thuong
    if ((sector_count - 1) > (UINT32_MAX - first_sector))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }
    if ((!s_is_high_capacity)
        && ((first_sector + sector_count - 1)
            > (UINT32_MAX / SD_CARD_BLOCK_SIZE)))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }

    for (uint32_t index = 0; index < sector_count; index++)
    {
        sd_card_result_t result = sd_card_write_block(
            &p_buffer[index * SD_CARD_BLOCK_SIZE], first_sector + index);
        if (result != SD_CARD_OK)
        {
            return result;
        }
    }

    return SD_CARD_OK;
}

// Tinh so sector 512 byte cua the tu CSD
sd_card_result_t sd_card_get_sector_count(uint32_t *p_sector_count)
{
    uint8_t csd[16];
    uint32_t c_size;
    uint64_t sectors;
    sd_card_result_t result;

    if (p_sector_count == 0)
    {
        return SD_CARD_INVALID_ARGUMENT;
    }
    if (!s_is_initialized)
    {
        return SD_CARD_NOT_READY;
    }

    result = sd_card_read_csd(csd);
    if (result != SD_CARD_OK)
    {
        return result;
    }

    // CSD_STRUCTURE (2 bit cao cua byte 0): 01 la CSD v2 (SDHC), con lai la CSD v1
    if ((csd[0] & 0xC0) == 0x40)
    {
        c_size = ((uint32_t)(csd[7] & 0x3F) << 16)
               | ((uint32_t)csd[8] << 8)
               | (uint32_t)csd[9];
        sectors = ((uint64_t)c_size + 1) * 1024;
    }
    else
    {
        uint32_t read_bl_len = (uint32_t)(csd[5] & 0x0F);
        uint32_t c_size_mult = ((uint32_t)(csd[9] & 0x03) << 1)
                             | ((uint32_t)(csd[10] & 0x80) >> 7);
        c_size = ((uint32_t)(csd[6] & 0x03) << 10)
               | ((uint32_t)csd[7] << 2)
               | ((uint32_t)(csd[8] & 0xC0) >> 6);

        if ((read_bl_len > 31) || (c_size_mult > 7))
        {
            return SD_CARD_ERROR;
        }

        sectors = ((uint64_t)c_size + 1)
                << (c_size_mult + 2 + read_bl_len);
        sectors /= SD_CARD_BLOCK_SIZE;
    }

    if ((sectors == 0) || (sectors > UINT32_MAX))
    {
        return SD_CARD_ERROR;
    }

    // Kiem tra o tren dam bao dung luong vua voi uint32_t
    *p_sector_count = (uint32_t)sectors;
    return SD_CARD_OK;
}

// Cho the ghi xong du lieu dang cho
sd_card_result_t sd_card_sync(void)
{
    bool is_ready;

    if (!s_is_initialized)
    {
        return SD_CARD_NOT_READY;
    }

    sd_card_select();
    is_ready = sd_card_wait_ready(SD_READY_TIMEOUT);
    sd_card_deselect();

    return is_ready ? SD_CARD_OK : SD_CARD_TIMEOUT;
}
