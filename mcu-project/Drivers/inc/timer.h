#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(uint32_t freq_hz);
void timer_wait_tick(void);

#endif /* TIMER_H */
