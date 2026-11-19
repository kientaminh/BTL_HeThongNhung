#include <Arduino.h>
#include <esp32-hal-rgb-led.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>

#include "config.h"
#include "status_led.h"

enum class LedMode
{
    Off,
    Audio,
    Download,
    Bootloader
};

static TaskHandle_t led_task_handle = nullptr;
static std::atomic<bool> upload_active{false};
static std::atomic<bool> download_active{false};
static std::atomic<bool> bootloader_active{false};

static void led_task(void *)
{
    constexpr unsigned long audio_hold_ms = 300;
    constexpr unsigned long blink_half_period_ms = 200;
    constexpr uint8_t brightness = 24;

    LedMode mode = LedMode::Off;
    unsigned long mode_started_ms = 0;
    unsigned long last_audio_ms = 0;
    bool audio_recent = false;
    bool lit = false;

    neopixelWrite(RGB_LED_GPIO, 0, 0, 0);

    // Keep blinking while the UART/I2S code or the STM32 programmer waits for I/O.
    for (;;)
    {
        const unsigned long now = millis();

        if (ulTaskNotifyTake(pdTRUE, 0) != 0)
        {
            last_audio_ms = now;
            audio_recent = true;
        }
        else if ((now - last_audio_ms) >= audio_hold_ms)
        {
            audio_recent = false;
        }

        LedMode next_mode = audio_recent ? LedMode::Audio : LedMode::Off;

        if (upload_active.load(std::memory_order_relaxed) ||
            download_active.load(std::memory_order_relaxed))
        {
            next_mode = LedMode::Download;
        }

        if (bootloader_active.load(std::memory_order_relaxed))
        {
            next_mode = LedMode::Bootloader;
        }

        const bool mode_changed = (next_mode != mode);

        if (mode_changed)
        {
            mode = next_mode;
            mode_started_ms = now;
        }

        const bool next_lit = (mode != LedMode::Off) &&
                              (((now - mode_started_ms) / blink_half_period_ms) % 2 == 0);

        if (mode_changed || (next_lit != lit))
        {
            lit = next_lit;
            const uint8_t red = (lit && (mode != LedMode::Audio)) ? brightness : 0;
            const uint8_t green = (lit && (mode != LedMode::Download)) ? brightness : 0;
            neopixelWrite(RGB_LED_GPIO, red, green, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

bool status_led_init()
{
    return xTaskCreate(led_task, "status_led", 2048, nullptr, 1, &led_task_handle) == pdPASS;
}

void status_led_audio_received()
{
    if (led_task_handle != nullptr)
    {
        xTaskNotifyGive(led_task_handle);
    }
}

void status_led_set_upload_active(bool active)
{
    upload_active.store(active, std::memory_order_relaxed);
}

void status_led_set_download_active(bool active)
{
    download_active.store(active, std::memory_order_relaxed);
}

void status_led_set_bootloader_active(bool active)
{
    bootloader_active.store(active, std::memory_order_relaxed);
}
