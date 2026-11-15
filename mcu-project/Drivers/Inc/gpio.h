#ifndef GPIO_H
#define GPIO_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx.h"

typedef enum {
    GPIO_MODE_INPUT_FLOATING,
    GPIO_MODE_INPUT_PULL_UP,
    GPIO_MODE_INPUT_PULL_DOWN,
    GPIO_MODE_OUTPUT_PUSH_PULL
} gpio_mode_t;

// port: GPIOA..GPIOE, pin: 0..15
void gpio_init(GPIO_TypeDef *port, uint8_t pin, gpio_mode_t mode);
bool gpio_read(GPIO_TypeDef *port, uint8_t pin);
void gpio_write(GPIO_TypeDef *port, uint8_t pin, bool state);

#endif /* GPIO_H */
