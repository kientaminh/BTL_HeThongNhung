#include <stdint.h>
#include "uart.h"

// Chuong trinh thu UART: gui loi chao roi phat lai (echo) moi byte nhan duoc
int main(void)
{
    static const uint8_t msg[] = "UART OK\r\n";

    uart_init(115200);

    uart_send_buffer(msg, sizeof(msg) - 1);

    for (;;)
    {
        uart_send_byte(uart_receive_byte());
    }
}
