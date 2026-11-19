#include "stm32_image.h"

#include <cassert>

int main()
{
    uint8_t vectors[STM32_VECTOR_BYTES] = {
        0x00, 0x50, 0x00, 0x20, // Initial SP = 0x20005000
        0x41, 0x00, 0x00, 0x08  // Thumb reset handler at 0x08000040
    };

    assert(stm32_image_valid(vectors, 1024));
    assert(!stm32_image_valid(vectors, 64));
    assert(!stm32_image_valid(vectors, STM32_IMAGE_MAX_BYTES + 1));
    assert(!stm32_image_valid(vectors, 7));
    vectors[4] &= 0xfe;
    assert(!stm32_image_valid(vectors, 1024));
    vectors[4] |= 1;
    vectors[1] = 0x51;
    assert(!stm32_image_valid(vectors, 1024));
}
