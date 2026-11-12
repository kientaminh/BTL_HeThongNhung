# Khung dự án STM32F103C8T6 dùng CMSIS và FreeRTOS

Các file driver và ứng dụng hiện vẫn rỗng. Header CMSIS đã được lấy từ kho chính thức bằng Git để viết driver truy cập thanh ghi trực tiếp, không dùng HAL hoặc LL. Chức năng của từng phần:

```text
mcu-project/
├── bsp/                    # Hỗ trợ board: ánh xạ chân và cấu hình riêng của board
├── cmsis/                  # Header thư viện từ Arm và ST, giữ nguyên nội dung gốc
│   ├── Core/
│   │   ├── Include/        # Header CMSIS-Core, có core_cm3.h và hỗ trợ compiler
│   │   └── LICENSE.txt     # Giấy phép của Arm
│   └── Device/
│       ├── Include/
│       │   ├── stm32f1xx.h         # Header chung, chọn thiết bị qua macro
│       │   ├── stm32f103xb.h       # Định nghĩa thanh ghi cho nhóm có STM32F103C8
│       │   └── system_stm32f1xx.h  # Khai báo SystemInit, SystemCoreClock và hàm liên quan
│       └── License.md      # Giấy phép của ST
├── drivers/                # Driver tự viết bằng thanh ghi qua định nghĩa CMSIS
│   ├── inc/                # Khai báo giao diện driver
│   │   ├── gpio.h          # Giao diện cấu hình, đọc và ghi chân GPIO
│   │   ├── uart.h          # Giao diện khởi tạo, truyền và nhận dữ liệu UART
│   │   └── i2c.h           # Giao diện khởi tạo và giao tiếp thiết bị qua I2C
│   └── src/                # Phần triển khai truy cập thanh ghi
│       ├── gpio.c          # Triển khai điều khiển GPIO
│       ├── uart.c          # Triển khai truyền và nhận UART
│       └── i2c.c           # Triển khai giao tiếp I2C
├── middlewares/
│   └── FreeRTOS/            # Dành cho kernel, port và heap; hiện còn rỗng
├── app/                    # Logic ứng dụng: các task FreeRTOS và trao đổi dữ liệu giữa task
│   ├── inc/                # Khai báo giao diện của ứng dụng
│   │   ├── task_sensor.h   # Khai báo task thu thập dữ liệu cảm biến
│   │   ├── task_display.h  # Khai báo task hiển thị dữ liệu
│   │   └── app_ipc.h       # Khai báo cơ chế trao đổi dữ liệu và đồng bộ giữa các task
│   └── src/                # Phần triển khai ứng dụng
│       ├── task_sensor.c   # Triển khai task đọc cảm biến và gửi dữ liệu
│       ├── task_display.c  # Triển khai task nhận dữ liệu và cập nhật hiển thị
│       └── app_ipc.c       # Khởi tạo, quản lý queue, semaphore hoặc mutex theo nhu cầu
├── main.c                  # Điểm vào: khởi tạo phần cứng, IPC, tạo task và gọi vTaskStartScheduler()
├── .gitignore              # Quy định các file và thư mục Git bỏ qua
└── README.md               # Mô tả cấu trúc và chức năng của từng phần
```

`bsp/` chứa thông tin phụ thuộc board cụ thể; `drivers/` cung cấp thao tác ngoại vi để `app/` sử dụng. `cmsis/` chỉ cung cấp header lõi và thiết bị, không chứa driver HAL hoặc LL. Ánh xạ chân và cấu hình clock chưa được xác định.

## Nguồn header CMSIS

Đã dùng `git clone --depth 1 --branch <tag> --filter=blob:none --sparse` và `git sparse-checkout set <thư-mục>` vào thư mục tạm, sau đó chép header và giấy phép vào dự án. Không thêm kho Git lồng nhau hoặc submodule.

| Thành phần | Kho nguồn | Tag | Commit | Phần đã chép |
| --- | --- | --- | --- | --- |
| CMSIS-Core | [ARM-software/CMSIS_5](https://github.com/ARM-software/CMSIS_5/tree/5.9.0) | `5.9.0` | `2b7495b8535bdcb306dac29b9ded4cfb679d7e5c` | Toàn bộ `CMSIS/Core/Include/` và `LICENSE.txt` |
| CMSIS Device F1 | [STMicroelectronics/cmsis-device-f1](https://github.com/STMicroelectronics/cmsis-device-f1/tree/v4.3.5) | `v4.3.5` | `8a76309ed1250d817e9c888c4417171d2ba3ba63` | Ba header trong `Device/Include/` và `License.md` |

## Khi cấu hình toolchain

- Định nghĩa macro thiết bị `STM32F103xB`: header chính thức của ST xếp STM32F103C8 vào nhóm này.
- Thêm `cmsis/Core/Include` và `cmsis/Device/Include` vào include path, tính từ `mcu-project/`.
- Trong driver, dùng `#include "stm32f1xx.h"` để lấy định nghĩa thanh ghi.
- Không định nghĩa `USE_HAL_DRIVER` khi viết trực tiếp bằng thanh ghi.

Chưa thêm startup, phần triển khai `system_stm32f1xx.c`, cấu hình linker, cấu hình build hoặc mã nguồn FreeRTOS. Khung hiện chưa phải dự án firmware có thể build; chưa build, flash hoặc chạy trên phần cứng.
