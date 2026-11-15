#include "sd_card.h"

#include "board.h"

#define SD_SPI_TIMEOUT          65535u
#define SD_READY_TIMEOUT        250000u
#define SD_INIT_RETRIES         1000u
#define SD_TOKEN_TIMEOUT        250000u
#define SD_DATA_TOKEN           0xFEu
#define SD_DATA_ACCEPTED        0x05u
#define SD_R1_IDLE              0x01u
#define SD_R1_ILLEGAL_COMMAND   0x04u
#define SD_CMD0                 0u
#define SD_CMD8                 8u
#define SD_CMD9                 9u
#define SD_CMD16                16u
#define SD_CMD17                17u
#define SD_CMD24                24u
#define SD_CMD55                55u
#define SD_CMD58                58u
#define SD_ACMD41               41u

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

static bool
sd_card_spi_transfer(uint8_t tx_data, uint8_t *p_rx_data)
{
    uint32_t timeout = SD_SPI_TIMEOUT;

    while (((BOARD_SD_SPI->SR & SPI_SR_TXE) == 0u) && (timeout > 0u))
    {
        timeout--;
    }

    if (timeout == 0u)
    {
        return false;
    }

    // Use 8-bit DR accesses because SPI is configured for 8-bit data frames.
    *(__IO uint8_t *)&BOARD_SD_SPI->DR = tx_data;
    timeout = SD_SPI_TIMEOUT;

    while (((BOARD_SD_SPI->SR & SPI_SR_RXNE) == 0u) && (timeout > 0u))
    {
        timeout--;
    }

    if (timeout == 0u)
    {
        return false;
    }

    *p_rx_data = *(__IO uint8_t *)&BOARD_SD_SPI->DR;
    return true;
}

static bool
sd_card_spi_receive(uint8_t *p_rx_data)
{
    return sd_card_spi_transfer(0xFFu, p_rx_data);
}

static void
sd_card_spi_set_speed(uint32_t baud_rate)
{
    BOARD_SD_SPI->CR1 &= ~SPI_CR1_SPE;
    BOARD_SD_SPI->CR1 = (BOARD_SD_SPI->CR1 & ~SPI_CR1_BR) | baud_rate;
    BOARD_SD_SPI->CR1 |= SPI_CR1_SPE;
}

static void
sd_card_spi_init(void)
{
    uint32_t pin_mask;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_SPI1EN;

    pin_mask = (0xFu << (BOARD_SD_CS_PIN * 4u))
             | (0xFu << (BOARD_SD_SCK_PIN * 4u))
             | (0xFu << (BOARD_SD_MISO_PIN * 4u))
             | (0xFu << (BOARD_SD_MOSI_PIN * 4u));
    BOARD_SD_PORT->CRL &= ~pin_mask;
    BOARD_SD_PORT->CRL |= (0x3u << (BOARD_SD_CS_PIN * 4u))
                        | (0xBu << (BOARD_SD_SCK_PIN * 4u))
                        | (0x4u << (BOARD_SD_MISO_PIN * 4u))
                        | (0xBu << (BOARD_SD_MOSI_PIN * 4u));
    BOARD_SD_PORT->BSRR = (1u << BOARD_SD_CS_PIN);

    BOARD_SD_SPI->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI
                      | SPI_CR1_BR;
    BOARD_SD_SPI->CR2 = 0u;
    BOARD_SD_SPI->CR1 |= SPI_CR1_SPE;
}

static void
sd_card_select(void)
{
    uint8_t unused;

    BOARD_SD_PORT->BRR = (1u << BOARD_SD_CS_PIN);
    (void)sd_card_spi_receive(&unused);
}

static void
sd_card_deselect(void)
{
    uint8_t unused;

    BOARD_SD_PORT->BSRR = (1u << BOARD_SD_CS_PIN);
    (void)sd_card_spi_receive(&unused);
}

static bool
sd_card_wait_ready(uint32_t timeout)
{
    uint8_t response = 0u;

    do
    {
        if (!sd_card_spi_receive(&response))
        {
            return false;
        }
        timeout--;
    } while ((response != 0xFFu) && (timeout > 0u));

    return (response == 0xFFu);
}

static uint8_t
sd_card_send_command(uint8_t command, uint32_t argument)
{
    uint8_t response = 0xFFu;
    uint8_t unused;
    uint8_t crc = 0x01u;

    sd_card_deselect();
    sd_card_select();

    if (command == SD_CMD0)
    {
        crc = 0x95u;
    }
    else if (command == SD_CMD8)
    {
        crc = 0x87u;
    }

    if (!sd_card_spi_transfer((uint8_t)(0x40u | command), &unused)
        || !sd_card_spi_transfer((uint8_t)(argument >> 24), &unused)
        || !sd_card_spi_transfer((uint8_t)(argument >> 16), &unused)
        || !sd_card_spi_transfer((uint8_t)(argument >> 8), &unused)
        || !sd_card_spi_transfer((uint8_t)argument, &unused)
        || !sd_card_spi_transfer(crc, &unused))
    {
        return 0xFFu;
    }

    for (uint32_t attempt = 0u; attempt < 10u; attempt++)
    {
        if (!sd_card_spi_receive(&response))
        {
            return 0xFFu;
        }

        if ((response & 0x80u) == 0u)
        {
            break;
        }
    }

    return response;
}

static bool
sd_card_read_data(uint8_t *p_buffer, uint32_t length)
{
    uint8_t token = 0xFFu;
    uint8_t crc;
    uint32_t timeout = SD_TOKEN_TIMEOUT;

    do
    {
        if (!sd_card_spi_receive(&token))
        {
            return false;
        }
        timeout--;
    } while ((token == 0xFFu) && (timeout > 0u));

    if ((token != SD_DATA_TOKEN) || (timeout == 0u))
    {
        return false;
    }

    for (uint32_t index = 0u; index < length; index++)
    {
        if (!sd_card_spi_receive(&p_buffer[index]))
        {
            return false;
        }
    }

    return sd_card_spi_receive(&crc) && sd_card_spi_receive(&crc);
}

static sd_card_result_t
sd_card_read_csd(uint8_t *p_csd)
{
    uint8_t response;

    response = sd_card_send_command(SD_CMD9, 0u);
    if (response != 0u)
    {
        sd_card_deselect();
        return SD_CARD_ERROR;
    }

    if (!sd_card_read_data(p_csd, 16u))
    {
        sd_card_deselect();
        return SD_CARD_TIMEOUT;
    }

    sd_card_deselect();
    return SD_CARD_OK;
}

static uint32_t
sd_card_sector_argument(uint32_t sector)
{
    if (s_is_high_capacity)
    {
        return sector;
    }

    return sector * SD_CARD_BLOCK_SIZE;
}

static sd_card_result_t
sd_card_read_block(uint8_t *p_buffer, uint32_t sector)
{
    uint8_t response;

    response = sd_card_send_command(SD_CMD17,
                                    sd_card_sector_argument(sector));
    if (response != 0u)
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

static sd_card_result_t
sd_card_write_block(const uint8_t *p_buffer, uint32_t sector)
{
    uint8_t response;
    uint8_t unused;

    response = sd_card_send_command(SD_CMD24,
                                    sd_card_sector_argument(sector));
    if (response != 0u)
    {
        sd_card_deselect();
        return SD_CARD_ERROR;
    }

    if (!sd_card_spi_transfer(SD_DATA_TOKEN, &unused))
    {
        sd_card_deselect();
        return SD_CARD_TIMEOUT;
    }

    for (uint32_t index = 0u; index < SD_CARD_BLOCK_SIZE; index++)
    {
        if (!sd_card_spi_transfer(p_buffer[index], &unused))
        {
            sd_card_deselect();
            return SD_CARD_TIMEOUT;
        }
    }

    if (!sd_card_spi_transfer(0xFFu, &unused)
        || !sd_card_spi_transfer(0xFFu, &unused)
        || !sd_card_spi_receive(&response))
    {
        sd_card_deselect();
        return SD_CARD_TIMEOUT;
    }

    if ((response & 0x1Fu) != SD_DATA_ACCEPTED)
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

sd_card_result_t
sd_card_init(void)
{
    uint8_t response;
    uint8_t unused;
    uint8_t ocr[4];
    bool is_v2 = false;

    s_is_initialized = false;
    s_is_high_capacity = false;
    sd_card_spi_init();
    sd_card_spi_set_speed(SPI_CR1_BR);
    sd_card_deselect();

    for (uint32_t index = 0u; index < 10u; index++)
    {
        if (!sd_card_spi_receive(&unused))
        {
            return SD_CARD_TIMEOUT;
        }
    }

    response = 0xFFu;
    for (uint32_t attempt = 0u; attempt < 10u; attempt++)
    {
        response = sd_card_send_command(SD_CMD0, 0u);
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

    response = sd_card_send_command(SD_CMD8, 0x000001AAu);
    if (response == SD_R1_IDLE)
    {
        for (uint32_t index = 0u; index < 4u; index++)
        {
            if (!sd_card_spi_receive(&ocr[index]))
            {
                sd_card_deselect();
                return SD_CARD_TIMEOUT;
            }
        }
        sd_card_deselect();

        if ((ocr[2] != 0x01u) || (ocr[3] != 0xAAu))
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

    response = SD_R1_IDLE;
    for (uint32_t attempt = 0u; attempt < SD_INIT_RETRIES; attempt++)
    {
        response = sd_card_send_command(SD_CMD55, 0u);
        if ((response != SD_R1_IDLE) && (response != 0u))
        {
            sd_card_deselect();
            return SD_CARD_ERROR;
        }

        response = sd_card_send_command(SD_ACMD41,
                                        is_v2 ? 0x40000000u : 0u);
        sd_card_deselect();
        if (response == 0u)
        {
            break;
        }
        if (response != SD_R1_IDLE)
        {
            return SD_CARD_ERROR;
        }
    }

    if (response != 0u)
    {
        return SD_CARD_TIMEOUT;
    }

    if (is_v2)
    {
        response = sd_card_send_command(SD_CMD58, 0u);
        if (response != 0u)
        {
            sd_card_deselect();
            return SD_CARD_ERROR;
        }

        for (uint32_t index = 0u; index < 4u; index++)
        {
            if (!sd_card_spi_receive(&ocr[index]))
            {
                sd_card_deselect();
                return SD_CARD_TIMEOUT;
            }
        }
        sd_card_deselect();

        s_is_high_capacity = ((ocr[0] & 0x40u) != 0u);
    }

    if (!s_is_high_capacity)
    {
        response = sd_card_send_command(SD_CMD16, SD_CARD_BLOCK_SIZE);
        sd_card_deselect();
        if (response != 0u)
        {
            return SD_CARD_ERROR;
        }
    }

    sd_card_spi_set_speed(SPI_CR1_BR_0);
    s_is_initialized = true;
    return SD_CARD_OK;
}

bool
sd_card_is_initialized(void)
{
    return s_is_initialized;
}

sd_card_result_t
sd_card_read_blocks(uint8_t *p_buffer,
                    uint32_t first_sector,
                    uint32_t sector_count)
{
    if ((p_buffer == 0) || (sector_count == 0u))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }
    if (!s_is_initialized)
    {
        return SD_CARD_NOT_READY;
    }
    if ((sector_count - 1u) > (UINT32_MAX - first_sector))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }
    if ((!s_is_high_capacity)
        && ((first_sector + sector_count - 1u)
            > (UINT32_MAX / SD_CARD_BLOCK_SIZE)))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }

    for (uint32_t index = 0u; index < sector_count; index++)
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

sd_card_result_t
sd_card_write_blocks(const uint8_t *p_buffer,
                     uint32_t first_sector,
                     uint32_t sector_count)
{
    if ((p_buffer == 0) || (sector_count == 0u))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }
    if (!s_is_initialized)
    {
        return SD_CARD_NOT_READY;
    }
    if ((sector_count - 1u) > (UINT32_MAX - first_sector))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }
    if ((!s_is_high_capacity)
        && ((first_sector + sector_count - 1u)
            > (UINT32_MAX / SD_CARD_BLOCK_SIZE)))
    {
        return SD_CARD_INVALID_ARGUMENT;
    }

    for (uint32_t index = 0u; index < sector_count; index++)
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

sd_card_result_t
sd_card_get_sector_count(uint32_t *p_sector_count)
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

    if ((csd[0] & 0xC0u) == 0x40u)
    {
        c_size = ((uint32_t)(csd[7] & 0x3Fu) << 16)
               | ((uint32_t)csd[8] << 8)
               | (uint32_t)csd[9];
        sectors = ((uint64_t)c_size + 1u) * 1024u;
    }
    else
    {
        uint32_t read_bl_len = (uint32_t)(csd[5] & 0x0Fu);
        uint32_t c_size_mult = ((uint32_t)(csd[9] & 0x03u) << 1)
                             | ((uint32_t)(csd[10] & 0x80u) >> 7);
        c_size = ((uint32_t)(csd[6] & 0x03u) << 10)
               | ((uint32_t)csd[7] << 2)
               | ((uint32_t)(csd[8] & 0xC0u) >> 6);

        if ((read_bl_len > 31u) || (c_size_mult > 7u))
        {
            return SD_CARD_ERROR;
        }

        sectors = ((uint64_t)c_size + 1u)
                << (c_size_mult + 2u + read_bl_len);
        sectors /= SD_CARD_BLOCK_SIZE;
    }

    if ((sectors == 0u) || (sectors > UINT32_MAX))
    {
        return SD_CARD_ERROR;
    }

    // The range check above guarantees that the capacity fits in uint32_t.
    *p_sector_count = (uint32_t)sectors;
    return SD_CARD_OK;
}

sd_card_result_t
sd_card_sync(void)
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
