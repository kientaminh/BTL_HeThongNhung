#include <stdint.h>
#include "uart.h"

// Chuong trinh thu UART: gui lap lai mot chuoi qua TX
int main(void)
{
    static const uint8_t msg[] = "UART OK\r\n";

    uart_init(115200);

    for (;;)
    {
        uart_send_buffer(msg, sizeof(msg) - 1);

        // Tre tho giua hai lan gui, khong can chinh xac
        for (volatile uint32_t i = 0; i < 2000000; i++)
        {
        }
    }
}
