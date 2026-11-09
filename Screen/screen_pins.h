#ifndef SECOND_TASK_SCREEN_PINS_H
#define SECOND_TASK_SCREEN_PINS_H

#include "stm32f4xx_hal.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* STM32F407 peripheral bit-band access for the example software buses. */
#define SCREEN_GPIO_BIT(reg, pin) (*(__IO uint32_t *)(PERIPH_BB_BASE + \
    (((uintptr_t)&(reg) - PERIPH_BASE) << 5U) + ((pin) << 2U)))
#define PBout(pin) SCREEN_GPIO_BIT(GPIOB->ODR, pin)
#define PBin(pin)  SCREEN_GPIO_BIT(GPIOB->IDR, pin)
#define PCout(pin) SCREEN_GPIO_BIT(GPIOC->ODR, pin)
#define PFout(pin) SCREEN_GPIO_BIT(GPIOF->ODR, pin)

#endif
