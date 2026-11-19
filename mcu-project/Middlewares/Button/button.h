#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

// Muc doc phai giu nguyen it nhat chung nay (ms) moi doi trang thai nut
#define BUTTON_DEBOUNCE_MS 20

// Chi so thong bao task danh cho ngat nut (chi so 0 da dung cho ngat DMA ADC)
#define BUTTON_NOTIFY_INDEX 1

void button_init(void);
void button_set_notify_task(TaskHandle_t task);
bool button_update(uint32_t elapsed_ms);
bool button_changing(void);

#endif /* BUTTON_H */
