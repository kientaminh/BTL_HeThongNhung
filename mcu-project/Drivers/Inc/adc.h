#ifndef ADC_H
#define ADC_H

#include <stdint.h>

// So mau trong 1 khoi DMA tra ve (128 mau = 16 ms o 8 kHz)
#define ADC_BLOCK_SAMPLES 128

void adc_init(void);
const uint16_t *adc_wait_block(void);

#endif /* ADC_H */
