#include "crc16.h"

// CRC-16/CCITT-FALSE: da thuc 0x1021, khong phan chieu bit, XOR cuoi 0 ("123456789" -> 0x29B1)
// Cung thuat toan voi AudioFrame::push() ben ESP32. Bat dau voi crc = CRC16_INIT,
// goi lai nhieu lan de tinh noi tiep. Tinh tung bit (khong bang) de khong ton 512 byte flash
uint16_t crc16_update(uint16_t crc, const uint8_t *p_data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)p_data[i] << 8);

        for (uint32_t bit = 0; bit < 8; bit++)
        {
            crc = ((crc & 0x8000) != 0) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }

    return crc;
}
