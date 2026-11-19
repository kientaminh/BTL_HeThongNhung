#include <Arduino.h>
#include <LittleFS.h>
#include <driver/gpio.h>

#include <atomic>

#include "config.h"
#include "status_led.h"
#include "stm32_image.h"
#include "stm32_programmer.h"

static constexpr uint32_t flash_base = 0x08000000;
static constexpr size_t flash_page_bytes = 1024;
static constexpr size_t block_bytes = 256;
static constexpr uint8_t ack = 0x79;

enum class ProgramState
{
    Idle, Queued, Running, InvalidImage, NoSync, WrongTarget, ReadDenied,
    EraseError, FileError, WriteError, VerifyError, BootPinsError, ResetError, Complete
};

static std::atomic<ProgramState> state{ProgramState::Idle};
static std::atomic<bool> program_from_ota{false};
// millis() cua khung am thanh hop le gan nhat, 0 = chua nhan khung nao
static std::atomic<uint32_t> last_audio_ms{0};
// Coi la loa con phat neu co khung trong khoang nay (STM32 gui 128 ms/khung)
static constexpr uint32_t audio_quiet_ms = 2000;

static constexpr gpio_num_t boot0_pin = static_cast<gpio_num_t>(STM32_BOOT0_GPIO);
static constexpr gpio_num_t nrst_pin = static_cast<gpio_num_t>(STM32_NRST_GPIO);

static bool configure_boot_pins()
{
    // Set output latches before enabling either driver; high on NRST is release.
    if (gpio_set_level(nrst_pin, 1) != ESP_OK || gpio_set_level(boot0_pin, 0) != ESP_OK)
    {
        return false;
    }

    gpio_config_t config = {};
    config.pin_bit_mask = 1ULL << nrst_pin;
    config.mode = GPIO_MODE_OUTPUT_OD;
    if (gpio_config(&config) != ESP_OK)
    {
        return false;
    }

    config.pin_bit_mask = 1ULL << boot0_pin;
    config.mode = GPIO_MODE_OUTPUT;
    return gpio_config(&config) == ESP_OK;
}

static bool reset_target(bool bootloader)
{
    if (gpio_set_level(nrst_pin, 0) != ESP_OK)
    {
        return false;
    }

    // BOOT0 must be stable when the STM32 samples it after reset release.
    const bool boot_set = gpio_set_level(boot0_pin, bootloader ? 1 : 0) == ESP_OK;
    delay(20);
    const bool released = gpio_set_level(nrst_pin, 1) == ESP_OK;

    if (bootloader && boot_set && released)
    {
        delay(100);
    }

    return boot_set && released;
}

static bool read_bytes(uint8_t *data, size_t count, unsigned long timeout_ms = 1000)
{
    const unsigned long start = millis();

    while (count > 0 && millis() - start < timeout_ms)
    {
        const int byte = Serial1.read();

        if (byte < 0)
        {
            delay(1);
            continue;
        }

        *data++ = static_cast<uint8_t>(byte);
        --count;
    }

    return count == 0;
}

static bool write_and_ack(const uint8_t *data, size_t count, unsigned long timeout_ms = 1000)
{
    uint8_t response = 0;
    return (Serial1.write(data, count) == count) &&
           read_bytes(&response, 1, timeout_ms) && (response == ack);
}

static bool command(uint8_t opcode)
{
    const uint8_t bytes[] = {opcode, static_cast<uint8_t>(opcode ^ 0xff)};
    return write_and_ack(bytes, sizeof(bytes));
}

static bool get_target()
{
    uint8_t id[4];
    if (!command(0x02) || !read_bytes(id, sizeof(id)) ||
        (id[0] != 1) || (id[1] != 0x04) || (id[2] != 0x10) || (id[3] != ack))
    {
        return false;
    }

    uint8_t count = 0;
    if (!command(0x00) || !read_bytes(&count, 1))
    {
        return false;
    }

    // Get returns N+1 bytes: version, then N supported opcodes.
    uint8_t commands[256];
    uint8_t end = 0;
    return read_bytes(commands, size_t(count) + 1) && read_bytes(&end, 1) && (end == ack) &&
           memchr(commands + 1, 0x11, count) && memchr(commands + 1, 0x31, count) &&
           memchr(commands + 1, 0x43, count);
}

static bool set_address(uint32_t address)
{
    uint8_t bytes[5] = {
        static_cast<uint8_t>(address >> 24), static_cast<uint8_t>(address >> 16),
        static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address), 0
    };
    bytes[4] = bytes[0] ^ bytes[1] ^ bytes[2] ^ bytes[3];
    return write_and_ack(bytes, sizeof(bytes));
}

static bool erase_image(size_t size)
{
    const size_t pages = (size + flash_page_bytes - 1) / flash_page_bytes;
    uint8_t bytes[STM32_IMAGE_MAX_BYTES / flash_page_bytes + 2];
    bytes[0] = static_cast<uint8_t>(pages - 1);
    uint8_t checksum = bytes[0];

    for (size_t i = 0; i < pages; ++i)
    {
        bytes[i + 1] = static_cast<uint8_t>(i);
        checksum ^= bytes[i + 1];
    }

    bytes[pages + 1] = checksum;
    return command(0x43) && write_and_ack(bytes, pages + 2, 8000);
}

static bool read_flash(uint32_t address, uint8_t *data, size_t count)
{
    const uint8_t n = static_cast<uint8_t>(count - 1);
    const uint8_t length[] = {n, static_cast<uint8_t>(n ^ 0xff)};
    return command(0x11) && set_address(address) && write_and_ack(length, sizeof(length)) &&
           read_bytes(data, count);
}

// Bo byte con trong bo dem RX (khung am thanh 460800 dang do, nhieu PA9 luc NRST)
// de khong bi doc nham thay cho ACK
static void flush_rx()
{
    while (Serial1.available() > 0)
    {
        (void)Serial1.read();
    }
}

// Mo Serial1 o baud, reset STM32 vao bootloader va gui 0x7F. AN3155: bootloader do baud
// tu byte nay va giu den lan reset sau, nen moi lan thu deu reset lai
static bool enter_bootloader(unsigned long baud)
{
    const uint8_t sync = 0x7f;

    Serial1.end();
    Serial1.begin(baud, SERIAL_8E1, UART_RX_GPIO, UART_TX_GPIO);

    if (!reset_target(true))
    {
        return false;
    }

    flush_rx();
    return write_and_ack(&sync, 1);
}

// Thu dong bo toi da 3 lan, moi lan reset lai. Tra baud dong bo duoc, 0 neu that bai
static unsigned long sync_bootloader()
{
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        if (enter_bootloader(STM32_BOOTLOADER_BAUD))
        {
            return STM32_BOOTLOADER_BAUD;
        }
    }

    return 0;
}

static ProgramState program_image(File &image, size_t size)
{
    if (!get_target())
    {
        return ProgramState::WrongTarget;
    }

    uint8_t readback[block_bytes];
    if (!read_flash(flash_base, readback, STM32_VECTOR_BYTES))
    {
        return ProgramState::ReadDenied;
    }

    if (!erase_image(size))
    {
        return ProgramState::EraseError;
    }

    uint8_t frame[block_bytes + 2];

    for (size_t offset = 0; offset < size; offset += block_bytes)
    {
        const size_t left = size - offset;
        const size_t actual = (left < block_bytes) ? left : block_bytes;
        const size_t padded = (actual + 3) & ~size_t(3);
        const uint32_t address = flash_base + offset;

        // AN3155 writes in multiples of four bytes; 0xff leaves tail padding erased.
        memset(frame + 1, 0xff, padded);

        if (image.read(frame + 1, actual) != actual)
        {
            return ProgramState::FileError;
        }

        frame[0] = static_cast<uint8_t>(padded - 1);
        uint8_t checksum = 0;

        for (size_t i = 0; i <= padded; ++i)
        {
            checksum ^= frame[i];
        }

        frame[padded + 1] = checksum;

        if (!command(0x31) || !set_address(address) || !write_and_ack(frame, padded + 2))
        {
            return ProgramState::WriteError;
        }

        if (!read_flash(address, readback, padded) || memcmp(readback, frame + 1, padded) != 0)
        {
            return ProgramState::VerifyError;
        }
    }

    return ProgramState::Complete;
}

bool stm32_program_busy()
{
    const ProgramState current = state.load();
    return (current == ProgramState::Queued) || (current == ProgramState::Running);
}

// Goi tu loop() moi khi nhan duoc 1 khung am thanh hop le
void stm32_program_note_audio()
{
    last_audio_ms.store(millis() | 1);
}

// Xep lich nap file upload (from_ota = false) hoac file OTA. Tu choi khi loa con phat:
// STM32 dang gui khung, reset luc nay cat ngang lan gui va lan dong file
Stm32ProgramRequest stm32_program_request(bool from_ota)
{
    const uint32_t last = last_audio_ms.load();

    if ((last != 0) && ((millis() - last) < audio_quiet_ms))
    {
        return Stm32ProgramRequest::AudioPlaying;
    }

    ProgramState current = state.load();

    if ((current == ProgramState::Queued) || (current == ProgramState::Running))
    {
        return Stm32ProgramRequest::Busy;
    }

    // Ghi nguon truoc khi doi trang thai: loop() chi doc nguon sau khi thay Queued
    program_from_ota.store(from_ota);
    return state.compare_exchange_strong(current, ProgramState::Queued) ?
           Stm32ProgramRequest::Accepted : Stm32ProgramRequest::Busy;
}

const char *stm32_program_status()
{
    switch (state.load())
    {
        case ProgramState::Idle: return "Chưa nạp.";
        case ProgramState::Queued: return "Đang chờ nạp.";
        case ProgramState::Running: return "Đang nạp và đọc lại Flash STM32…";
        case ProgramState::InvalidImage: return "File đã lưu không hợp lệ hoặc không đọc được.";
        case ProgramState::NoSync: return "STM32 không trả ACK sau 3 lần thử. Kiểm tra dây BOOT0, NRST và UART.";
        case ProgramState::WrongTarget: return "ID STM32 hoặc bộ lệnh bootloader không phù hợp.";
        case ProgramState::ReadDenied: return "Không đọc được Flash STM32. Chưa xóa Flash.";
        case ProgramState::EraseError: return "Xóa Flash thất bại. Thử nạp lại từ trang.";
        case ProgramState::FileError: return "Đọc file ESP32 bị lỗi sau khi xóa Flash. Hãy upload và nạp lại.";
        case ProgramState::WriteError: return "Ghi STM32 thất bại. Thử nạp lại từ trang.";
        case ProgramState::VerifyError: return "Đọc lại Flash không khớp. Thử nạp lại từ trang.";
        case ProgramState::BootPinsError: return "Không điều khiển được BOOT0 hoặc NRST.";
        case ProgramState::ResetError: return "Đã xác minh Flash nhưng không điều khiển được tín hiệu khởi động lại.";
        case ProgramState::Complete: return "Nạp thành công. ESP32 đã phát tín hiệu khởi động lại STM32.";
    }

    return "Không rõ trạng thái nạp.";
}

bool stm32_program_run_pending()
{
    ProgramState expected = ProgramState::Queued;

    if (!state.compare_exchange_strong(expected, ProgramState::Running))
    {
        return false;
    }

    const char *path = program_from_ota.load() ? STM32_OTA_IMAGE_PATH : STM32_LOCAL_IMAGE_PATH;
    File image = LittleFS.open(path, FILE_READ);
    const size_t size = image ? image.size() : 0;
    uint8_t vectors[STM32_VECTOR_BYTES];

    if (!image || image.read(vectors, sizeof(vectors)) != sizeof(vectors) ||
        !stm32_image_valid(vectors, size) || !image.seek(0))
    {
        state.store(ProgramState::InvalidImage);
        return true;
    }

    // Called from loop(), which also owns Serial1 during audio reception.
    status_led_set_bootloader_active(true);
    ProgramState result = ProgramState::BootPinsError;
    unsigned long baud = 0;

    if (configure_boot_pins())
    {
        baud = sync_bootloader();
        result = (baud != 0) ? program_image(image, size) : ProgramState::NoSync;
    }

    Serial1.end();
    Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX_GPIO, UART_TX_GPIO);

    if ((result == ProgramState::Complete) && !reset_target(false))
    {
        result = ProgramState::ResetError;
    }

    status_led_set_bootloader_active(false);
    image.close();
    state.store(result);
    Serial.printf("%s (%s, %lu baud)\n", stm32_program_status(), path, baud);
    return true;
}
