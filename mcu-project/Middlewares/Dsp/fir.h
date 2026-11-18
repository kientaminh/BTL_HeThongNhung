#ifndef FIR_H
#define FIR_H

#include <stdint.h>

// So he so FIR (so le, pha tuyen tinh: tre (FIR_TAPS - 1) / 2 = 31 mau = 3.9 ms o 8 kHz)
#define FIR_TAPS 63

void fir_reset(void);
void fir_process(int16_t *p_samples, uint32_t count);

#endif /* FIR_H */
