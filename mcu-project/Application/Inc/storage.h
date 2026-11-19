#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stdint.h>
#include "adc.h"

// Moi bo dem PCM chua dung 1 khoi DMA (da doi sang 16 bit va loc)
#define STORAGE_BUFFER_SAMPLES ADC_BLOCK_SAMPLES

// Thong ke task record gui kem lenh STOP de storage in ra UART
typedef struct
{
    uint32_t overruns;           // so lan DMA ghi de khoi chua lay
    uint32_t dropped;            // so khoi bo vi pool het bo dem (the ghi cham)
    uint32_t record_stack_free;  // word stack con trong it nhat cua task record
} storage_stats_t;

void storage_init(void);
bool storage_start_recording(void);
int16_t *storage_get_buffer(void);
void storage_submit_buffer(int16_t *p_pcm);
bool storage_write_failed(void);
void storage_stop_recording(const storage_stats_t *p_stats);

#endif /* STORAGE_H */
