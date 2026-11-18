#include "fir.h"

// FIR thong thap cat 1 kHz o 8 kHz: sinc cua so Hamming, cung cong thuc Tools/uart_to_wav.py
// (CUTOFF_HZ = 1000). He so Q15, tong = 32768 nen do loi DC = 1. Doi xung nen khong can dao thu tu
static const int16_t fir_coeffs[FIR_TAPS] = {
    -19, -29, -23, 0, 32, 55, 48, 0, -72,
    -123, -104, 0, 148, 247, 204, 0, -277, -455,
    -372, 0, 499, 819, 674, 0, -938, -1598, -1392,
    0, 2411, 5175, 7371, 8206, 7371, 5175, 2411, 0,
    -1392, -1598, -938, 0, 674, 819, 499, 0, -372,
    -455, -277, 0, 204, 247, 148, 0, -104, -123,
    -72, 0, 48, 55, 32, 0, -23, -29, -19
};

// Mau cu giu lai giua cac lan goi (moi mau ghi 2 lan, cach nhau FIR_TAPS) de doan
// fir_history[pos + 1 .. pos + FIR_TAPS] luon lien tuc, khong can chia lay du trong vong nhan
static int16_t fir_history[2 * FIR_TAPS];
static uint32_t fir_pos = 0;

// Xoa mau cu: goi truoc moi doan ghi moi de 31 mau dau khong lan tin hieu cua doan truoc
void fir_reset(void)
{
    for (uint32_t i = 0; i < (2 * FIR_TAPS); i++)
    {
        fir_history[i] = 0;
    }

    fir_pos = 0;
}

// Loc tai cho count mau 16 bit co dau. Trang thai noi tiep giua cac khoi nen goi lien tuc
// cho cung 1 dong tin hieu; chi 1 kenh (trang thai dung chung)
void fir_process(int16_t *p_samples, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        // Tro vong nguoc: mau moi nhat nam o dau doan [fir_pos .. fir_pos + FIR_TAPS - 1]
        fir_pos = (fir_pos == 0) ? (FIR_TAPS - 1) : (fir_pos - 1);
        fir_history[fir_pos] = p_samples[i];
        fir_history[fir_pos + FIR_TAPS] = p_samples[i];

        // Tong |he so| = 54376 nen |acc| < 32768 * 54376 < 2^31, int32_t khong tran
        int32_t acc = 1 << 14;    // lam tron khi dich Q15
        const int16_t *p_x = &fir_history[fir_pos];

        for (uint32_t k = 0; k < FIR_TAPS; k++)
        {
            acc += (int32_t)fir_coeffs[k] * p_x[k];
        }

        // Dich phai so am: GCC dich so hoc (implementation-defined theo chuan C)
        acc >>= 15;

        // Gon song (ringing) quanh canh doc co the vuot 16 bit: bao hoa thay vi tran
        if (acc > INT16_MAX)
        {
            acc = INT16_MAX;
        }
        else if (acc < INT16_MIN)
        {
            acc = INT16_MIN;
        }

        p_samples[i] = (int16_t)acc;
    }
}
