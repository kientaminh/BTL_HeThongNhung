#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

// Gia tri dau cua CRC-16/CCITT-FALSE
#define CRC16_INIT 0xFFFF

uint16_t crc16_update(uint16_t crc, const uint8_t *p_data, uint32_t len);

#endif /* CRC16_H */
