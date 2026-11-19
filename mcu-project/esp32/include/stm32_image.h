#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr size_t STM32_IMAGE_MAX_BYTES = 64 * 1024;
constexpr size_t STM32_VECTOR_BYTES = 8;

inline uint32_t stm32_read_le32(const uint8_t *bytes)
{
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) |
           (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

inline bool stm32_image_valid(const uint8_t *vectors, size_t size)
{
    if ((size < STM32_VECTOR_BYTES) || (size > STM32_IMAGE_MAX_BYTES))
    {
        return false;
    }

    // Match Linker/stm32f103c8.ld. The initial stack may point one byte past SRAM.
    constexpr uint32_t flash_base = 0x08000000;
    const uint32_t stack = stm32_read_le32(vectors);
    const uint32_t reset = stm32_read_le32(vectors + 4);
    const uint32_t reset_address = reset & ~uint32_t(1);

    return (stack > 0x20000000) && (stack <= 0x20005000) && ((stack % 8) == 0) &&
           ((reset & 1) != 0) && (reset_address >= flash_base + STM32_VECTOR_BYTES) &&
           (reset_address < flash_base + size);
}
