#include "timer.h"
#include "board.h"

// Khoi tao timer tao nhip deu freq_hz lan moi giay (freq_hz khac 0, toi da BOARD_TIM_CLK_HZ)
void timer_init(uint32_t freq_hz)
{
    // Bat clock TIM3 (bit 1: TIM3EN), di cung BOARD_TIM trong board.h
    RCC->APB1ENR |= (1 << 1);

    // So xung clock trong 1 nhip = (PSC + 1) * (ARR + 1)
    // PSC va ARR deu 16 bit: chon PSC nho nhat de ARR khong vuot 65535
    uint32_t ticks = BOARD_TIM_CLK_HZ / freq_hz;
    uint32_t psc = ticks >> 16;

    BOARD_TIM->PSC = psc;
    BOARD_TIM->ARR = (ticks / (psc + 1)) - 1;

    // Tao update event de nap PSC ngay (bit 0: UG), roi xoa co UIF no vua dat
    BOARD_TIM->EGR = (1 << 0);
    BOARD_TIM->SR = ~(1 << 0);

    // Phat TRGO moi lan timer tran: MMS = 010 (bit 6:4 cua CR2), dung de kich ADC
    // Dat sau UG de update event o tren khong kich ADC som
    BOARD_TIM->CR2 = (2 << 4);

    // Bat dau dem (bit 0: CEN)
    BOARD_TIM->CR1 = (1 << 0);
}

// Cho den nhip ke tiep (ham chan)
void timer_wait_tick(void)
{
    while ((BOARD_TIM->SR & (1 << 0)) == 0)
    {
        // Cho UIF = 1 (bit 0 cua SR)
    }

    // Xoa UIF bang cach ghi 0, cac co khac ghi 1 de giu nguyen
    BOARD_TIM->SR = ~(1 << 0);
}
