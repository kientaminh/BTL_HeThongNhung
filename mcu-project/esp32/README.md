# ESP32-S3 N16R8 — nhận và phát âm thanh

Đổi chân trong `include/config.h`. Chạy `pio run` từ thư mục này để build.

| ESP32-S3 | Kết nối |
|---|---|
| GPIO18 / UART1 RX | STM32 PA9 / USART1 TX |
| GPIO17 / UART1 TX | STM32 PA10 / USART1 RX nếu cần chiều ngược |
| GPIO7 | MAX98357A BCLK |
| GPIO15 | MAX98357A LRC / WS |
| GPIO16 | MAX98357A DIN |
| GND | GND STM32 và MAX98357A |

Nối loa giữa OUT+ và OUT−. GAIN có thể để hở; SD chỉ để hở nếu module có sẵn mạch kéo để bật amp.

## Khung UART

UART 460800, 8N1. Một khung gồm 2054 byte theo thứ tự:

1. Header: `A5 5A C3 3C`.
2. 2048 byte PCM: 1024 mẫu mono 8 kHz, signed 16-bit, byte thấp trước.
3. 2 byte CRC-16/CCITT-FALSE của phần PCM, byte thấp trước. Polynomial `0x1021`, giá trị đầu `0xFFFF`, không phản chiếu bit, XOR cuối `0x0000`.

Gửi từng khung liền mạch, không chèn log bên trong khung. Log giữa hai khung được bỏ qua. Với luồng ghi âm 8 kHz, một khối tương ứng 128 ms. ESP32 chỉ đưa khung đủ và đúng CRC vào I²S. Source STM32 hiện mới gửi log qua UART, nên cần bổ sung bên gửi theo định dạng trên để phát từ STM32.

Kiểm tra bộ đọc khung trên máy tính bằng `c++ -std=c++11 -Iinclude tests/audio_frame_test.cpp -o /tmp/audio_frame_test && /tmp/audio_frame_test`.
