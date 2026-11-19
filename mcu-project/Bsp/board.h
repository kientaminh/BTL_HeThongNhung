#ifndef BOARD_H
#define BOARD_H

#include "stm32f1xx.h"

// Log debug len may tinh: USART2 mac dinh (khong remap): TX = PA2, RX = PA3
#define BOARD_LOG_USART     USART2
#define BOARD_LOG_PORT      GPIOA
#define BOARD_LOG_TX_PIN    2
#define BOARD_LOG_RX_PIN    3
// Clock cap cho USART2: APB1 = 36 MHz
#define BOARD_LOG_PCLK_HZ   36000000

// ESP32-S3: USART1 mac dinh (khong remap): TX = PA9, RX = PA10
// Cung la cong bootloader ROM: ESP nap firmware qua day (ESP dieu khien BOOT0, NRST)
// Khong dat log hay du lieu khac len 2 chan nay
#define BOARD_ESP_USART     USART1
#define BOARD_ESP_PORT      GPIOA
#define BOARD_ESP_TX_PIN    9
#define BOARD_ESP_RX_PIN    10
// Clock cap cho USART1: APB2 = SYSCLK = 72 MHz
#define BOARD_ESP_PCLK_HZ   72000000

// Mic MAX9814: chan OUT noi vao PA0 = ADC1 kenh 0 (ADC12_IN0)
#define BOARD_ADC           ADC1
#define BOARD_MIC_PORT      GPIOA
#define BOARD_MIC_PIN       0
#define BOARD_MIC_ADC_CH    0

// Timer tao nhip: TIM3 (sau nay kich duoc ADC1 qua TIM3 TRGO)
#define BOARD_TIM           TIM3
// Clock cap cho TIM3: APB1 = 36 MHz, timer duoc nhan 2 vi APB1 chia 2 -> 72 MHz
#define BOARD_TIM_CLK_HZ    72000000

// Clock cap cho SysTick: HCLK = SYSCLK = 72 MHz (AHB khong chia)
#define BOARD_HCLK_HZ       72000000

// The microSD card uses SPI1 in SPI mode. Keep these pins reserved for storage.
#define BOARD_SD_SPI           SPI1
#define BOARD_SD_PORT          GPIOA
#define BOARD_SD_CS_PIN        4
#define BOARD_SD_SCK_PIN       5
#define BOARD_SD_MISO_PIN      6
#define BOARD_SD_MOSI_PIN      7

// Nut ghi am: PA1 noi len 3.3 V, keo xuong ben trong (nhan = muc cao)
#define BOARD_BTN_PORT      GPIOA
#define BOARD_BTN_PIN       1

// LED bao dang ghi: PC13, sang o muc thap
#define BOARD_LED_PORT      GPIOC
#define BOARD_LED_PIN       13
#endif /* BOARD_H */
