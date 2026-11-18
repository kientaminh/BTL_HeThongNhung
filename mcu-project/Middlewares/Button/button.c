#include "button.h"
#include "board.h"
#include "gpio.h"

// Trang thai da chong doi, true = dang nhan
static bool stable_state = false;
// Muc doc lan truoc va thoi gian muc do giu nguyen
static bool last_raw = false;
static uint32_t stable_ms = 0;

// Nut noi chan len 3.3 V, keo xuong ben trong: nhan = muc cao
void button_init(void)
{
    gpio_init(BOARD_BTN_PORT, BOARD_BTN_PIN, GPIO_MODE_INPUT_PULL_DOWN);
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
