#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

// So mau trong 1 khoi DMA tra ve (1024 mau = 128 ms o 8 kHz)
// Khoi dai de chiu duoc luc the SD ghi cham; bo dem DMA chiem 2 khoi = 4096 byte RAM
#define ADC_BLOCK_SAMPLES 1024

void adc_init(void);
void adc_set_recorder_task(TaskHandle_t task);
const uint16_t *adc_wait_block(void);
void adc_reset_buffer(void);
uint32_t adc_get_overrun_count(void);

#endif /* ADC_H */
