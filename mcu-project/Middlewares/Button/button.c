#include "button.h"
#include <stddef.h>
#include "board.h"
#include "gpio.h"

// Ngat EXTI1_IRQHandler ben duoi chi dung cho chan so 1 (EXTI line 0..4 moi line 1 IRQ rieng)
#if (BOARD_BTN_PIN != 1)
#error "Doi BOARD_BTN_PIN thi phai doi EXTI1_IRQHandler/EXTI1_IRQn theo chan moi"
#endif

// Trang thai da chong doi, true = dang nhan
static bool stable_state = false;
// Muc doc lan truoc va thoi gian muc do giu nguyen
static bool last_raw = false;
static uint32_t stable_ms = 0;
// Task duoc ngat nut danh thuc (ghi 1 lan khi task chay, ISR doc)
static TaskHandle_t volatile notify_task = NULL;

// Nut noi chan len 3.3 V, keo xuong ben trong: nhan = muc cao
// Bat ngat EXTI ca 2 canh de task cho nut ngu han thay vi doc nut moi 5 ms
// Goi truoc vTaskStartScheduler(); ngat den truoc khi co task dang ky thi bo qua
void button_init(void)
{
    gpio_init(BOARD_BTN_PORT, BOARD_BTN_PIN, GPIO_MODE_INPUT_PULL_DOWN);

    // Bat clock AFIO (bit 0: AFIOEN) de chon port cho duong EXTI
    RCC->APB2ENR |= (1 << 0);

    // Duong EXTI1 lay tu port A: EXTICR1 bit 7:4 = 0000 (dung voi BOARD_BTN_PORT = GPIOA)
    AFIO->EXTICR[0] &= ~(0xF << (BOARD_BTN_PIN * 4));

    // Ngat ca canh len (nhan) va canh xuong (tha): bit TR1 cua RTSR va FTSR
    EXTI->RTSR |= (1 << BOARD_BTN_PIN);
    EXTI->FTSR |= (1 << BOARD_BTN_PIN);

    // Xoa co cho (ghi 1 vao PR1) roi mo mat na ngat (bit MR1 cua IMR)
    EXTI->PR = (1 << BOARD_BTN_PIN);
    EXTI->IMR |= (1 << BOARD_BTN_PIN);

    // ISR goi API FreeRTOS nen uu tien khong duoc cao hon max syscall (5), giong ngat DMA
    NVIC_SetPriority(EXTI1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

// Dang ky task nhan thong bao BUTTON_NOTIFY_INDEX moi khi chan nut doi muc
void button_set_notify_task(TaskHandle_t task)
{
    notify_task = task;
}

// Ngat chan nut doi muc (ke ca nay doi): chi xoa co va danh thuc task, khong chong doi o day
void EXTI1_IRQHandler(void)
{
    BaseType_t higher_priority_woken = pdFALSE;

    // Xoa co cho bang cach ghi 1 vao bit PR1
    EXTI->PR = (1 << BOARD_BTN_PIN);

    if (notify_task != NULL)
    {
        vTaskNotifyGiveIndexedFromISR(notify_task, BUTTON_NOTIFY_INDEX, &higher_priority_woken);
        portYIELD_FROM_ISR(higher_priority_woken);
    }
}

// Doc nut 1 lan va tra ve trang thai da chong doi. elapsed_ms: thoi gian tu lan goi truoc
// Trang thai chi doi khi muc doc giu nguyen it nhat BUTTON_DEBOUNCE_MS. Goi thua hon
// BUTTON_DEBOUNCE_MS van dung: khi do can 2 lan doc giong nhau lien tiep
bool button_update(uint32_t elapsed_ms)
{
    bool raw = gpio_read(BOARD_BTN_PORT, BOARD_BTN_PIN);

    if (raw != last_raw)
    {
        last_raw = raw;
        stable_ms = 0;
    }
    else if (stable_ms < BUTTON_DEBOUNCE_MS)
    {
        // Chi cong toi nguong de stable_ms khong tran
        stable_ms += elapsed_ms;
    }

    if (stable_ms >= BUTTON_DEBOUNCE_MS)
    {
        stable_state = raw;
    }

    return stable_state;
}

// true khi muc doc lan cuoi khac trang thai da chong doi: dang cho het nay, can doc tiep
// false: chan dang on dinh, co the ngu cho ngat EXTI
bool button_changing(void)
{
    return (last_raw != stable_state);
}
