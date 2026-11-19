#pragma once

constexpr int UART_RX_GPIO = 18;
constexpr int UART_TX_GPIO = 17;
constexpr unsigned long UART_BAUD = 460800;
// Bootloader ROM STM32F1 tu do baud tu byte 0x7F; 115200 la muc cao nhat ST da kiem thu (AN3155)
constexpr unsigned long STM32_BOOTLOADER_BAUD = 115200;
constexpr int STM32_BOOT0_GPIO = 9;
constexpr int STM32_NRST_GPIO = 10;

constexpr int I2S_BCLK_GPIO = 7;
constexpr int I2S_WS_GPIO = 15;
constexpr int I2S_DOUT_GPIO = 16;

constexpr unsigned AUDIO_SAMPLE_RATE_HZ = 8000;

constexpr int RGB_LED_GPIO = 48;
