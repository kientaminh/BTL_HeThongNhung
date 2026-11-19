#include "uart.h"
#include "board.h"

// Phan cung cua 1 cong UART
typedef struct
{
    USART_TypeDef *p_usart;
    GPIO_TypeDef *p_port;
    uint8_t tx_pin;
    uint8_t rx_pin;
    uint32_t pclk_hz;
    volatile uint32_t *p_rcc_enr;   // thanh ghi bat clock USART (APB1ENR hoac APB2ENR)
    uint8_t rcc_bit;                // bit bat clock USART trong thanh ghi do
} uart_hw_t;

static const uart_hw_t uart_hw[UART_PORT_COUNT] = {
    // USART2 nam tren APB1 (bit 17: USART2EN)
    [UART_LOG] = { BOARD_LOG_USART, BOARD_LOG_PORT, BOARD_LOG_TX_PIN, BOARD_LOG_RX_PIN,
                   BOARD_LOG_PCLK_HZ, &RCC->APB1ENR, 17 },
    // USART1 nam tren APB2 (bit 14: USART1EN)
    [UART_ESP] = { BOARD_ESP_USART, BOARD_ESP_PORT, BOARD_ESP_TX_PIN, BOARD_ESP_RX_PIN,
                   BOARD_ESP_PCLK_HZ, &RCC->APB2ENR, 14 },
};

// Ghi 4 bit cau hinh (CNF[1:0]:MODE[1:0]) cua 1 chan: chan 0..7 trong CRL, 8..15 trong CRH
static void uart_config_pin(GPIO_TypeDef *p_port, uint8_t pin, uint32_t cfg)
{
    volatile uint32_t *p_cr = (pin < 8) ? &p_port->CRL : &p_port->CRH;
    uint32_t shift = (uint32_t)(pin % 8) * 4;

    *p_cr = (*p_cr & ~(0xF << shift)) | (cfg << shift);
}

// Khoi tao UART phat va thu (TX, RX), khung 8N1
void uart_init(uart_port_t port, uint32_t baud)
{
    const uart_hw_t *p_hw = &uart_hw[port];

    // Bat clock port GPIO (IOPAEN la bit 2 cua APB2ENR, cac port ke tiep cach nhau 0x400)
    // va clock USART
    RCC->APB2ENR |= (1 << (2 + (((uint32_t)p_hw->p_port - (uint32_t)GPIOA) / 0x400)));
    *p_hw->p_rcc_enr |= (1 << p_hw->rcc_bit);

    // TX: alternate function push-pull, 50 MHz (MODE = 11, CNF = 10 -> 0xB)
    // RX: input pull-up (MODE = 00, CNF = 10 -> 0x8) de chan khong bat nhieu khi ho day
    uart_config_pin(p_hw->p_port, p_hw->tx_pin, 0xB);
    uart_config_pin(p_hw->p_port, p_hw->rx_pin, 0x8);
    // ODR = 1 chon keo len (0 la keo xuong)
    p_hw->p_port->BSRR = (1 << p_hw->rx_pin);

    // BRR = PCLK / baud, lam tron gan nhat (36 MHz / 460800 -> 78, lech +0.16 %)
    p_hw->p_usart->BRR = (p_hw->pclk_hz + (baud / 2)) / baud;

    // 8 bit du lieu, khong parity, 1 stop la mac dinh sau reset
    // Bat thu (bit 2: RE), bat phat (bit 3: TE) va bat USART (bit 13: UE)
    p_hw->p_usart->CR1 = (1 << 2) | (1 << 3) | (1 << 13);
}

// Gui 1 byte: cho thanh ghi DR trong roi ghi
void uart_send_byte(uart_port_t port, uint8_t data)
{
    USART_TypeDef *p_usart = uart_hw[port].p_usart;

    while ((p_usart->SR & (1 << 7)) == 0)
    {
        // Cho TXE = 1 (bit 7 cua SR)
    }

    p_usart->DR = data;
}

// Nhan 1 byte: cho co du lieu roi doc DR (ham chan, khong timeout)
// Doc SR roi doc DR cung xoa luon co loi ORE/FE/NE neu co
uint8_t uart_receive_byte(uart_port_t port)
{
    USART_TypeDef *p_usart = uart_hw[port].p_usart;

    while ((p_usart->SR & (1 << 5)) == 0)
    {
        // Cho RXNE = 1 (bit 5 cua SR)
    }

    return (uint8_t)p_usart->DR;
}

// Gui mot mang byte
void uart_send_buffer(uart_port_t port, const uint8_t *p_buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
    {
        uart_send_byte(port, p_buf[i]);
    }
}
