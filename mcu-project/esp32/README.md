# ESP32-S3 N16R8 — nhận và phát âm thanh

Đổi chân trong `include/config.h`. Chạy `pio run` từ thư mục này để build.

| ESP32-S3 | Kết nối |
|---|---|
| GPIO18 / UART1 RX | STM32 PA9 / USART1 TX |
| GPIO17 / UART1 TX | STM32 PA10 / USART1 RX (bắt buộc: bootloader ROM nhận lệnh nạp qua chân này) |
| GPIO7 | MAX98357A BCLK |
| GPIO15 | MAX98357A LRC / WS |
| GPIO16 | MAX98357A DIN |
| GND | GND STM32 và MAX98357A |

Nối loa giữa OUT+ và OUT−. GAIN có thể để hở; SD chỉ để hở nếu module có sẵn mạch kéo để bật amp.

## LED RGB GPIO48

- Nháy xanh lá khi nhận khung PCM hợp lệ. Tắt sau khoảng 300 ms không có khung mới.
- Nháy đỏ khi tải file firmware STM32 qua mạng, ưu tiên hơn màu xanh.
- Nháy vàng trong phiên nạp STM32 và ưu tiên hơn hai màu còn lại. Khi thêm luồng nạp, gọi `status_led_set_bootloader_active(true)` lúc bắt đầu và `status_led_set_bootloader_active(false)` trên mọi đường kết thúc, kể cả lỗi.

## Khung UART

UART 460800, 8N1. Một khung gồm 2054 byte theo thứ tự:

1. Header: `A5 5A C3 3C`.
2. 2048 byte PCM: 1024 mẫu mono 8 kHz, signed 16-bit, byte thấp trước.
3. 2 byte CRC-16/CCITT-FALSE của phần PCM, byte thấp trước. Polynomial `0x1021`, giá trị đầu `0xFFFF`, không phản chiếu bit, XOR cuối `0x0000`.

Gửi từng khung liền mạch, không chèn log bên trong khung. Log giữa hai khung được bỏ qua. Với luồng ghi âm 8 kHz, một khối tương ứng 128 ms. ESP32 chỉ đưa khung đủ và đúng CRC vào I²S. Trên STM32, `esp_link` gửi các khung này qua USART1.

Kiểm tra bộ đọc khung trên máy tính bằng `c++ -std=c++11 -Iinclude tests/audio_frame_test.cpp -o /tmp/audio_frame_test && /tmp/audio_frame_test`.

## Tải firmware STM32 qua Wi‑Fi

1. Sao chép `include/ota_local.h.example` thành `include/ota_local.h`. Điền Wi‑Fi, URL HTTPS của manifest và chứng chỉ gốc PEM dùng để xác thực server. File cục bộ được Git bỏ qua. Nếu thiếu file, ESP32 vẫn chạy phần âm thanh.
2. ESP32 cần phân vùng LittleFS với nhãn `spiffs` có sẵn trong bảng phân vùng hiện tại. Nếu chưa có hệ thống tệp, có thể tạo thư mục `data/` rỗng và dùng tác vụ PlatformIO `uploadfs` để khởi tạo một lần. Tác vụ này ghi đè dữ liệu cũ trong phân vùng đó, nên sao lưu trước nếu có dữ liệu. Firmware không tự format nếu mount không thành công.
3. Đưa `Bin/stm32.bin` lên máy chủ trước, sau đó đăng manifest. `size` là số byte thực của file (tối đa 65536) và `sha256` là chuỗi 64 ký tự hex từ `sha256sum Bin/stm32.bin`.

| Trường manifest | Ví dụ / yêu cầu |
|---|---|
| `target` | `STM32F103C8T6` |
| `version` | `1.2.0` |
| `url` | URL HTTPS trực tiếp của file `.bin` |
| `size` | Kích thước file theo byte |
| `sha256` | SHA-256 hex của toàn bộ file |

Cả hai URL cần trả HTTP 200 và `Content-Length`, phục vụ nội dung gốc (không nén, không redirect hoặc chunked). ESP32 chờ Wi‑Fi và đồng bộ giờ qua SNTP, rồi kiểm tra manifest mỗi 60 giây. LED đỏ chỉ chạy khi tải file `.bin`. File hợp lệ được công bố tại `/stm32.ready.bin`; file lỗi hoặc tải thiếu không thay thế file đã lưu. ESP32 bỏ qua tải lại nếu bản đang lưu khớp manifest.

Đây là bước tải và chuẩn bị file trên ESP32. Việc chuyển UART sang giao thức bootloader ST và nạp file vào STM32 thuộc bước nạp riêng.

## Upload local từ trình duyệt

1. Kết nối Wi-Fi `STM32-OTA` (không mật khẩu), rồi mở `http://192.168.4.1/`.
2. Chọn `Bin/stm32.bin` và bấm **Upload firmware**. Không cần Internet.
3. LED đỏ nháy trong lúc nhận dữ liệu. Trang báo kết quả sau khi ESP32 kiểm tra kích thước và vector khởi động rồi lưu file ở `/stm32.local.ready.bin`. File upload lỗi không thay thế bản đã lưu.

Cần khởi tạo LittleFS một lần như hướng dẫn ở trên. File local có đường dẫn riêng với file tải từ HTTPS. Sau khi upload, chọn nguồn file (upload từ trình duyệt hoặc file OTA đã kiểm tra) rồi bấm **Nạp STM32**. Trang từ chối nạp khi đang có upload dở.

### Nạp STM32 tự động qua bootloader ROM

Ngắt nguồn trước khi nối dây. Cả hai board phải dùng mức logic 3,3 V và nối chung GND. Giữ các dây UART trong bảng trên, rồi nối thêm:

| ESP32-S3 | STM32F103C8T6 |
|---|---|
| GPIO9 | Chân giữa của cột BOOT0. Tháo jumper BOOT0 và nối điện trở 10 kΩ từ chân giữa xuống GND. |
| GPIO10 | Chân `R` (NRST) trên Blue Pill. ESP32 dùng open-drain; giữ mạch kéo lên và nút RESET có sẵn. |

Giữ jumper **BOOT1 = 0**. Các GPIO nằm trong `include/config.h`.

Chỉ nạp khi loa đã phát xong: ESP32 từ chối nạp nếu còn nhận khung âm thanh trong 2 giây gần nhất. Trên trang local, bấm **Nạp STM32**. ESP32 tự giữ NRST thấp, đặt BOOT0 cao, nhả NRST, xóa byte cũ trong bộ đệm nhận rồi nạp qua UART 115200 8E1 (tốc độ cao nhất ST đã kiểm thử, AN3155). Không đồng bộ được thì tự reset và thử lại, tối đa 3 lần. Sau khi đọc lại và xác minh Flash thành công, ESP32 trả UART về 460800 8N1, đưa BOOT0 thấp và phát xung reset để chạy ứng dụng. Nếu nạp lỗi, ESP32 giữ chế độ bootloader và lần bấm tiếp theo sẽ tự reset vào bootloader lại.

Wi-Fi mở. Thiết bị nào kết nối được vào mạng `STM32-OTA` đều có thể upload file và yêu cầu nạp STM32. Khi có `ota_local.h`, ESP32 ngắt kết nối router trong lúc có thiết bị nối vào `STM32-OTA` để AP không bị đổi kênh giữa chừng; OTA chỉ chạy khi AP không có ai. Giữ bản firmware cũ ở máy tính để upload lại nếu cần.
