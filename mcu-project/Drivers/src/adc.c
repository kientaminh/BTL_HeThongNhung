#include "adc.h"
#include "board.h"

// DMA ghi vong tron vao 2 nua, moi nua la 1 khoi ADC_BLOCK_SAMPLES mau
static uint16_t adc_buf[2 * ADC_BLOCK_SAMPLES];

// Khoi tao ADC doc 1 kenh (mic): moi lan TIM3 tran (TRGO) chuyen doi 1 mau,
// DMA chep mau vao adc_buf
// Goi sau timer_init() va truoc timer_start(): trigger chi duoc bat khi timer con dung
void adc_init(void)
{
    // Bat clock DMA1 (bit 0: DMA1EN). Tren F1, ADC1 gan co dinh voi DMA1 kenh 1
    RCC->AHBENR |= (1 << 0);

    // Nguon: thanh ghi DR cua ADC, dich: adc_buf, so mau moi vong
    DMA1_Channel1->CPAR = (uint32_t)&BOARD_ADC->DR;
    DMA1_Channel1->CMAR = (uint32_t)adc_buf;
    DMA1_Channel1->CNDTR = 2 * ADC_BLOCK_SAMPLES;

    // Vong tron (bit 5: CIRC), tang dia chi bo nho (bit 7: MINC),
    // ngoai vi 16 bit (PSIZE = 01 tai bit 9:8), bo nho 16 bit (MSIZE = 01 tai bit 11:10)
    // DIR = 0 (bit 4): doc tu ngoai vi. Bat kenh (bit 0: EN)
    DMA1_Channel1->CCR = (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10) | (1 << 0);

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

    // Kich bang TIM3 TRGO: EXTSEL = 100 (bit 19:17), bat EXTTRIG (bit 20)
    // Chuyen doi don: CONT = 0 (bit 1), moi trigger chi 1 mau
    // Moi mau xong thi yeu cau DMA doc DR (bit 8: DMA)
    // Bat nguon ADC (bit 0: ADON), lan ghi dau chi danh thuc ADC
    BOARD_ADC->CR2 = (4 << 17) | (1 << 20) | (1 << 8) | (1 << 0);

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

// Cho DMA ghi xong 1 khoi, tra ve con tro toi ADC_BLOCK_SAMPLES mau 12 bit (ham chan)
// Khoi chi hop le den khi DMA quay lai ghi de: phai xu ly xong trong 1 khoi thoi gian
const uint16_t *adc_wait_block(void)
{
    for (;;)
    {
        uint32_t isr = DMA1->ISR;

        // Nua dau day (bit 2: HTIF1), xoa co bang CHTIF1 (bit 2 cua IFCR)
        if ((isr & (1 << 2)) != 0)
        {
            DMA1->IFCR = (1 << 2);
            return &adc_buf[0];
        }

        // Nua sau day (bit 1: TCIF1), xoa co bang CTCIF1 (bit 1 cua IFCR)
        if ((isr & (1 << 1)) != 0)
        {
            DMA1->IFCR = (1 << 1);
            return &adc_buf[ADC_BLOCK_SAMPLES];
        }
    }
}
