#ifndef BOARD_H
#define BOARD_H

#include "stm32f1xx.h"

// USART1 mac dinh (khong remap): TX = PA9, RX = PA10
#define BOARD_USART         USART1
#define BOARD_USART_PORT    GPIOA
#define BOARD_USART_TX_PIN  9
#define BOARD_USART_RX_PIN  10
// Clock cap cho USART1: APB2 = SYSCLK = 72 MHz 
#define BOARD_USART_PCLK_HZ 72000000

// Mic MAX9814: chan OUT noi vao PA0 = ADC1 kenh 0 (ADC12_IN0)
#define BOARD_ADC           ADC1
#define BOARD_MIC_PORT      GPIOA
#define BOARD_MIC_PIN       0
#define BOARD_MIC_ADC_CH    0

// Timer tao nhip: TIM3 (sau nay kich duoc ADC1 qua TIM3 TRGO)
#define BOARD_TIM           TIM3
// Clock cap cho TIM3: APB1 = 36 MHz, timer duoc nhan 2 vi APB1 chia 2 -> 72 MHz
#define BOARD_TIM_CLK_HZ    72000000

#endif /* BOARD_H */
