#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

// Muc doc phai giu nguyen it nhat chung nay (ms) moi doi trang thai nut
#define BUTTON_DEBOUNCE_MS 20

void button_init(void);
bool button_update(uint32_t elapsed_ms);

#endif /* BUTTON_H */
