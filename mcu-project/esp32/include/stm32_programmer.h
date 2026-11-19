#pragma once

// File firmware STM32 da kiem tra, chon mot trong hai nguon khi nap
constexpr char STM32_LOCAL_IMAGE_PATH[] = "/stm32.local.ready.bin";    // upload tu trinh duyet
constexpr char STM32_OTA_IMAGE_PATH[] = "/stm32.ready.bin";            // tai qua HTTPS

enum class Stm32ProgramRequest
{
    Accepted,
    Busy,
    AudioPlaying
};

Stm32ProgramRequest stm32_program_request(bool from_ota);
bool stm32_program_busy();
const char *stm32_program_status();
bool stm32_program_run_pending();
void stm32_program_note_audio();
