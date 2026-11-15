#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include <stdint.h>

#define SD_CARD_BLOCK_SIZE 512u

typedef enum
{
    SD_CARD_OK = 0,
    SD_CARD_ERROR,
    SD_CARD_TIMEOUT,
    SD_CARD_NOT_READY,
    SD_CARD_INVALID_ARGUMENT
} sd_card_result_t;

sd_card_result_t sd_card_init(void);
bool sd_card_is_initialized(void);
sd_card_result_t sd_card_read_blocks(uint8_t *p_buffer,
                                     uint32_t first_sector,
                                     uint32_t sector_count);
sd_card_result_t sd_card_write_blocks(const uint8_t *p_buffer,
                                      uint32_t first_sector,
                                      uint32_t sector_count);
sd_card_result_t sd_card_get_sector_count(uint32_t *p_sector_count);
sd_card_result_t sd_card_sync(void);

#endif /* SD_CARD_H */
