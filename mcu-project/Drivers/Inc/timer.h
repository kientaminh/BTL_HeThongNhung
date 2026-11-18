#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(uint32_t freq_hz);
void timer_start(void);
void timer_stop(void);
void timer_wait_tick(void);
void timer_delay_ms(uint32_t ms);

#endif /* TIMER_H */
