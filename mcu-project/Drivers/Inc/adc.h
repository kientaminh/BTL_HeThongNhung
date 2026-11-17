#ifndef ADC_H
#define ADC_H

#include <stdint.h>

// So mau trong 1 khoi DMA tra ve (1024 mau = 128 ms o 8 kHz)
// Khoi dai de chiu duoc luc the SD ghi cham; bo dem DMA chiem 2 khoi = 4096 byte RAM
#define ADC_BLOCK_SAMPLES 1024

void adc_init(void);
const uint16_t *adc_wait_block(void);

#endif /* ADC_H */
