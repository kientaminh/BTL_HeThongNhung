#include "uart.h"
#include "board.h"

// Khoi tao UART chi phat (TX), khung 8N1
void uart_init(uint32_t baud)
{
    // Bat clock GPIOA (bit 2: IOPAEN) va USART1 (bit 14: USART1EN)
    // Di cung BOARD_USART, BOARD_USART_PORT trong board.h
    RCC->APB2ENR |= (1 << 2) | (1 << 14);

    // TX: alternate function push-pull, 50 MHz (MODE = 11, CNF = 10 -> 0xB)
    // Chan 8..15 nam trong CRH, moi chan 4 bit
    uint32_t shift = (BOARD_USART_TX_PIN - 8) * 4;
    uint32_t mask = 0xF;  // 4 bit cau hinh cua chan
    uint32_t af_pp = 0xB; // MODE = 11, CNF = 10
    BOARD_USART_PORT->CRH = (BOARD_USART_PORT->CRH & ~(mask << shift)) | (af_pp << shift);

    // BRR = PCLK / baud, lam tron gan nhat (72 MHz / 115200 = 625)
    BOARD_USART->BRR = (BOARD_USART_PCLK_HZ + (baud / 2)) / baud;

    // 8 bit du lieu, khong parity, 1 stop la mac dinh sau reset
    // Bat phat (bit 3: TE) va bat USART (bit 13: UE)
    BOARD_USART->CR1 = (1 << 3) | (1 << 13);
}

// Gui 1 byte: cho thanh ghi DR trong roi ghi
void uart_send_byte(uint8_t data)
{
    while ((BOARD_USART->SR & (1 << 7)) == 0)
    {
        // Cho TXE = 1 (bit 7 cua SR)
    }

    BOARD_USART->DR = data;
}

// Gui mot mang byte
void uart_send_buffer(const uint8_t *p_buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        uart_send_byte(p_buf[i]);
    }
}
