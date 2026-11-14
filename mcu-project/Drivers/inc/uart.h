#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(uint32_t baud);
void uart_send_byte(uint8_t data);
void uart_send_buffer(const uint8_t *p_buf, uint32_t len);

#endif /* UART_H */
