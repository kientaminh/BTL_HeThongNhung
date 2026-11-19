#!/usr/bin/env python3
"""Thu mic qua UART roi ghi 2 file WAV 16 bit mono tu mau 12 bit (0..4095).

file.wav: mau tho, khong chinh sua.
file_loc.wav: loc trung vi 3 diem, FIR thong thap, bo muc DC roi nhan 16 (12 bit -> 16 bit).
Dung: python3 uart_to_wav.py [cong] [file.wav]
"""
import math
import os
import struct
import sys
import termios
import tty
import wave

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
OUT = sys.argv[2] if len(sys.argv) > 2 else "mic.wav"
SAMPLE_RATE = 8000  # phai khop timer_init() trong Src/main.c
SECONDS = 5
TOTAL = SAMPLE_RATE * SECONDS
CUTOFF_HZ = 1500  # tan so cat cua FIR thong thap
FIR_TAPS = 63  # so le; vung chuyen tiep rong khoang 3.3 * SAMPLE_RATE / FIR_TAPS = 420 Hz


def median3(x):
    """Loc trung vi 3 diem: bo gai dai 1 mau, giu nguyen 2 mau o hai dau."""
    return [x[0]] + [sorted(x[i - 1:i + 2])[1] for i in range(1, len(x) - 1)] + [x[-1]]


def fir_lowpass(x):
    """FIR thong thap sinc cua so Hamming, do loi DC = 1, da bu tre (dau ra cung do dai)."""
    m = FIR_TAPS // 2
    fc = CUTOFF_HZ / SAMPLE_RATE
    h = [(2 * fc if k == 0 else math.sin(2 * math.pi * fc * k) / (math.pi * k))
         * (0.54 + 0.46 * math.cos(math.pi * k / m)) for k in range(-m, m + 1)]
    total = sum(h)
    h = [v / total for v in h]
    # Lap mau dau va cuoi de hai dau khong bi keo ve 0
    p = [x[0]] * m + x + [x[-1]] * m
    return [round(sum(a * b for a, b in zip(h, p[i:i + FIR_TAPS]))) for i in range(len(x))]


def write_wav(path, x):
    with wave.open(path, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(SAMPLE_RATE)
        wav.writeframes(struct.pack("<%dh" % len(x), *x))
    print("%s: %d mau, min %d, max %d" % (path, len(x), min(x), max(x)))


fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY)
tty.setraw(fd)
attrs = termios.tcgetattr(fd)
attrs[4] = attrs[5] = termios.B460800  # phai khop uart_init() trong Src/main.c
attrs[6][termios.VMIN] = 0
attrs[6][termios.VTIME] = 10  # read() tra ve rong sau 1 s khong co du lieu
termios.tcsetattr(fd, termios.TCSANOW, attrs)
termios.tcflush(fd, termios.TCIFLUSH)  # bo du lieu cu con nam trong bo dem

# Moi mau 2 byte: byte 1 = 1 (bit 7) + 5 bit cao, byte 2 = 0 (bit 7) + 7 bit thap
samples = []
hi = None
while len(samples) < TOTAL:
    data = os.read(fd, 4096)
    if not data:
        sys.exit("khong co du lieu tu " + PORT)
    for b in data:
        if b & 0x80:
            hi = b & 0x1F
        elif hi is not None:
            samples.append((hi << 7) | b)
            hi = None
os.close(fd)
del samples[TOTAL:]

write_wav(OUT, samples)
filtered = fir_lowpass(median3(samples))
dc = round(sum(filtered) / TOTAL)
# Nhan 16 trai 12 bit ra 16 bit; kep lai de mau lech xa muc DC khong tran int16
write_wav(os.path.splitext(OUT)[0] + "_loc.wav",
          [max(-32768, min(32767, (v - dc) * 16)) for v in filtered])
