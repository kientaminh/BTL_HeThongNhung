#include <stdint.h>
#include "adc.h"
#include "uart.h"

// Gui so thap phan khong dau kem xuong dong
static void send_number(uint16_t value)
{
    uint8_t digits[5]; // uint16_t toi da 5 chu so
    uint32_t n = 0;

    do
    {
        digits[n++] = (uint8_t)('0' + (value % 10));
        value /= 10;
    } while (value != 0);

    while (n > 0)
    {
        uart_send_byte(digits[--n]);
    }

    uart_send_buffer((const uint8_t *)"\r\n", 2);
}

// Chuong trinh thu ADC: in gia tri mic (0..4095) qua UART
int main(void)
{
    uart_init(115200);
    adc_init();

    for (;;)
    {
        send_number(adc_read());

        // Tre tho giua hai lan in, khong can chinh xac
        for (volatile uint32_t i = 0; i < 500000; i++)
        {
        }
    }
}
