#include "gpio.h"

// Cau hinh 1 chan (pin 0..15) theo mode, tu bat clock cho port
void gpio_init(GPIO_TypeDef *port, uint8_t pin, gpio_mode_t mode)
{
    // Moi chan 4 bit (CNF[1:0]:MODE[1:0]): chan 0..7 trong CRL, 8..15 trong CRH
    volatile uint32_t *cr = (pin < 8) ? &port->CRL : &port->CRH;
    uint32_t shift = (uint32_t)(pin % 8) * 4;
    uint32_t cfg = 0;

    // Bat clock port: IOPAEN la bit 2 cua APB2ENR, cac port ke tiep cach nhau 0x400
    RCC->APB2ENR |= ((uint32_t)1 << (2 + (((uint32_t)port - (uint32_t)GPIOA) / 0x400)));

    switch (mode)
    {
        case GPIO_MODE_INPUT_FLOATING:
            // CNF = 01, MODE = 00
            cfg = 0x4;
            break;

        case GPIO_MODE_INPUT_PULL_UP:
            // CNF = 10, MODE = 00, ODR = 1 chon keo len (ghi qua BSRR bit pin)
            cfg = 0x8;
            port->BSRR = ((uint32_t)1 << pin);
            break;

        case GPIO_MODE_INPUT_PULL_DOWN:
            // CNF = 10, MODE = 00, ODR = 0 chon keo xuong (ghi qua BSRR bit pin + 16)
            cfg = 0x8;
            port->BSRR = ((uint32_t)1 << (pin + 16));
            break;

        case GPIO_MODE_OUTPUT_PUSH_PULL:
            // CNF = 00, MODE = 10: output push-pull, toc do 2 MHz
            cfg = 0x2;
            break;

        default:
            // Mode khong hop le: de chan o input floating (trang thai sau reset)
            cfg = 0x4;
            break;
    }

    *cr = (*cr & ~((uint32_t)0xF << shift)) | (cfg << shift);
}

// Doc muc logic tren chan (IDR), true = muc cao
bool gpio_read(GPIO_TypeDef *port, uint8_t pin)
{
    return ((port->IDR & ((uint32_t)1 << pin)) != 0);
}

// Dat muc ra cua chan qua BSRR (ghi nguyen tu, khong can doc-sua-ghi ODR):
// bit pin = set, bit pin + 16 = reset
void gpio_write(GPIO_TypeDef *port, uint8_t pin, bool state)
{
    port->BSRR = state ? ((uint32_t)1 << pin) : ((uint32_t)1 << (pin + 16));
}
