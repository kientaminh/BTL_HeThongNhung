#ifndef UART_H
#define UART_H

#include <stdint.h>

// Cong UART tren board (chan, clock trong board.h)
typedef enum
{
    UART_LOG,           // USART2 PA2/PA3: log debug len may tinh
    UART_ESP,           // USART1 PA9/PA10: ESP32-S3 (cung la cong bootloader ROM)
    UART_PORT_COUNT
} uart_port_t;

void uart_init(uart_port_t port, uint32_t baud);
void uart_send_byte(uart_port_t port, uint8_t data);
uint8_t uart_receive_byte(uart_port_t port);
void uart_send_buffer(uart_port_t port, const uint8_t *p_buf, uint32_t len);

#endif /* UART_H */
