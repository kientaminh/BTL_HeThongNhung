#include "adc.h"
#include "board.h"

// Khoi tao ADC doc 1 kenh (mic), chuyen doi don, kich bang phan mem
void adc_init(void)
{
    // Bat clock GPIOA (bit 2: IOPAEN) va ADC1 (bit 9: ADC1EN)
    // Di cung BOARD_ADC, BOARD_MIC_PORT trong board.h
    RCC->APB2ENR |= (1 << 2) | (1 << 9);

    // ADCCLK = PCLK2 / 6 = 12 MHz (toi da 14 MHz): ADCPRE = 10 tai bit 15:14
    RCC->CFGR = (RCC->CFGR & ~(3 << 14)) | (2 << 14);

    // Chan mic: analog input (MODE = 00, CNF = 00 -> 0x0)
    // Chan 0..7 nam trong CRL, moi chan 4 bit
    BOARD_MIC_PORT->CRL &= ~(0xF << (BOARD_MIC_PIN * 4));

    // Thoi gian lay mau 55.5 chu ky (SMP = 101), kenh 0..9 nam trong SMPR2, moi kenh 3 bit
    // Mot lan chuyen doi = (55.5 + 12.5) / 12 MHz = 5.67 us
    BOARD_ADC->SMPR2 = (BOARD_ADC->SMPR2 & ~(7 << (BOARD_MIC_ADC_CH * 3)))
                     | (5 << (BOARD_MIC_ADC_CH * 3));

    // Chuoi regular dai 1 (L = 0000 tai bit 23:20 cua SQR1), SQ1 = kenh mic (bit 4:0 cua SQR3)
    BOARD_ADC->SQR1 &= ~(0xF << 20);
    BOARD_ADC->SQR3 = BOARD_MIC_ADC_CH;

    // Kich bang SWSTART: EXTSEL = 111 (bit 19:17), bat EXTTRIG (bit 20)
    // Bat nguon ADC (bit 0: ADON), lan ghi dau chi danh thuc ADC
    BOARD_ADC->CR2 = (7 << 17) | (1 << 20) | (1 << 0);

    // Cho ADC on dinh (tSTAB = 1 us) truoc khi hieu chuan, khong can chinh xac
    for (volatile uint32_t i = 0; i < 1000; i++)
    {
    }

    // Xoa gia tri hieu chuan cu (bit 3: RSTCAL), cho bit tu xoa
    BOARD_ADC->CR2 |= (1 << 3);
    while ((BOARD_ADC->CR2 & (1 << 3)) != 0)
    {
    }

    // Hieu chuan (bit 2: CAL), cho bit tu xoa
    BOARD_ADC->CR2 |= (1 << 2);
    while ((BOARD_ADC->CR2 & (1 << 2)) != 0)
    {
    }
}

// Doc 1 mau 12 bit (0..4095): kich chuyen doi roi cho xong (ham chan)
uint16_t adc_read(void)
{
    // Bat dau chuyen doi (bit 22: SWSTART)
    BOARD_ADC->CR2 |= (1 << 22);

    while ((BOARD_ADC->SR & (1 << 1)) == 0)
    {
        // Cho EOC = 1 (bit 1 cua SR)
    }

    // Doc DR tu xoa EOC
    return (uint16_t)BOARD_ADC->DR;
}
