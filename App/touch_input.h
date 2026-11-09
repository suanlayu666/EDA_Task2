#ifndef SECOND_TASK_TOUCH_INPUT_H
#define SECOND_TASK_TOUCH_INPUT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint16_t x;
    uint16_t y;
} TouchPoint;

typedef enum
{
    TOUCH_DOWN,
    TOUCH_MOVE,
    TOUCH_UP
} TouchEventType;

typedef struct
{
    TouchEventType type;
    TouchPoint point;
} TouchEvent;

bool TouchInput_Init(void);
bool TouchInput_Poll(bool raw, TouchEvent *event);

#endif
