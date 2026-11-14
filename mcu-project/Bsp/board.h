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

#endif /* BOARD_H */
