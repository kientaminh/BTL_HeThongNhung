#include <Arduino.h>
#include <LittleFS.h>
#include <driver/i2s.h>

#include "audio_frame.h"
#include "config.h"
#include "local_upload.h"
#include "ota.h"
#include "status_led.h"
#include "stm32_programmer.h"

static constexpr i2s_port_t audio_port = I2S_NUM_0;
static constexpr unsigned long frame_gap_ms = 100;

static AudioFrame audio_frame;
static int16_t stereo[AudioFrame::sample_count * 2];
static unsigned long last_byte_ms;

static void audio_init()
{
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = AUDIO_SAMPLE_RATE_HZ;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.dma_buf_count = 6;
    config.dma_buf_len = 256;
    config.tx_desc_auto_clear = true;

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = I2S_BCLK_GPIO;
    pins.ws_io_num = I2S_WS_GPIO;
    pins.data_out_num = I2S_DOUT_GPIO;
    pins.data_in_num = I2S_PIN_NO_CHANGE;

    ESP_ERROR_CHECK(i2s_driver_install(audio_port, &config, 0, nullptr));
    ESP_ERROR_CHECK(i2s_set_pin(audio_port, &pins));
}

void setup()
{
    Serial.begin(115200);
    audio_init();

    if (!status_led_init())
    {
        Serial.println("RGB status task did not start");
    }

    // One audio block is 2048 bytes; UART can keep receiving while I2S drains DMA.
    Serial1.setRxBufferSize(8192);
    Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX_GPIO, UART_TX_GPIO);
    const bool storage_ready = LittleFS.begin(false);
    ota_init(storage_ready);
    local_upload_init(storage_ready);
}

void loop()
{
    if (stm32_program_run_pending())
    {
        audio_frame.reset();
        return;
    }

    if ((millis() - last_byte_ms) >= frame_gap_ms)
    {
        audio_frame.reset();
    }

    const int byte = Serial1.read();

    if (byte < 0)
    {
        delay(1);
        return;
    }

    last_byte_ms = millis();

    if (!audio_frame.push(static_cast<uint8_t>(byte)))
    {
        return;
    }

    status_led_audio_received();
    stm32_program_note_audio();    // chan nap STM32 khi loa con dang phat

    // Duplicate mono so the amp plays the same signal with either channel selected.
    for (size_t i = 0; i < AudioFrame::sample_count; ++i)
    {
        const int16_t sample = audio_frame.sample(i);
        stereo[i * 2] = sample;
        stereo[i * 2 + 1] = sample;
    }

    size_t written = 0;
    const esp_err_t result = i2s_write(audio_port, stereo, sizeof(stereo), &written,
                                      pdMS_TO_TICKS(250));

    if ((result != ESP_OK) || (written != sizeof(stereo)))
    {
        Serial.println("I2S audio block incomplete");
    }
}
