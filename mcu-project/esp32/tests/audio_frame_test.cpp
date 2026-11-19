#include "audio_frame.h"

#include <cassert>

static bool send_frame(AudioFrame &frame, uint8_t crc_low)
{
    const uint8_t sync[] = {0xa5, 0x5a, 0xc3, 0x3c};

    for (uint8_t byte : sync)
    {
        assert(!frame.push(byte));
    }

    // Signed samples -32768 and 32767, then silence. CRC-16/CCITT-FALSE = 0x6c03.
    const uint8_t samples[] = {0x00, 0x80, 0xff, 0x7f};

    for (size_t i = 0; i < AudioFrame::payload_bytes; ++i)
    {
        assert(!frame.push(i < sizeof(samples) ? samples[i] : 0));
    }

    assert(!frame.push(crc_low));
    return frame.push(0x6c);
}

int main()
{
    AudioFrame frame;

    for (char byte : "start\r\nsamples: 1024\r\n")
    {
        assert(!frame.push(static_cast<uint8_t>(byte)));
    }

    assert(!frame.push(0xa5));
    assert(!send_frame(frame, 0x02));
    assert(send_frame(frame, 0x03));
    assert(frame.sample(0) == -32768);
    assert(frame.sample(1) == 32767);
    assert(frame.sample(2) == 0);
}
