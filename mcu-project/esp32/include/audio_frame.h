#pragma once

#include <stddef.h>
#include <stdint.h>

class AudioFrame
{
public:
    static constexpr size_t sample_count = 1024;
    static constexpr size_t payload_bytes = sample_count * 2;

    void reset()
    {
        sync_size = 0;
        payload_size = 0;
        crc = 0xffff;
    }

    bool push(uint8_t byte)
    {
        constexpr uint8_t sync[] = {0xa5, 0x5a, 0xc3, 0x3c};

        if (sync_size < sizeof(sync))
        {
            if (byte == sync[sync_size])
            {
                ++sync_size;
            }
            else
            {
                sync_size = (byte == sync[0]) ? 1 : 0;
            }

            return false;
        }

        if (payload_size < payload_bytes)
        {
            payload[payload_size++] = byte;
            crc ^= static_cast<uint16_t>(byte) << 8;

            for (unsigned bit = 0; bit < 8; ++bit)
            {
                crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
            }

            return false;
        }

        if (payload_size == payload_bytes)
        {
            crc_low = byte;
            ++payload_size;
            return false;
        }

        const uint16_t received_crc = crc_low | (static_cast<uint16_t>(byte) << 8);
        const bool valid = received_crc == crc;
        reset();
        return valid;
    }

    int16_t sample(size_t index) const
    {
        const uint16_t value = payload[index * 2] |
                               (static_cast<uint16_t>(payload[index * 2 + 1]) << 8);
        return (value < 0x8000) ? value : (static_cast<int32_t>(value) - 65536);
    }

private:
    uint8_t payload[payload_bytes];
    size_t sync_size = 0;
    size_t payload_size = 0;
    uint16_t crc = 0xffff;
    uint8_t crc_low = 0;
};
