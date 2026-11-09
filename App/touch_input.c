#include "touch_input.h"

#include "touch.h"

#define TOUCH_SCAN_INTERVAL_MS 15U

static bool was_down;
static TouchPoint last_point;
static uint32_t last_scan_ms;

bool TouchInput_Init(void)
{
    was_down = false;
    last_scan_ms = HAL_GetTick();
    return TP_InitWithoutCalibration() == 0;
}

bool TouchInput_Poll(bool raw, TouchEvent *event)
{
    const uint32_t now = HAL_GetTick();
    if (now - last_scan_ms < TOUCH_SCAN_INTERVAL_MS)
    {
        return false;
    }
    last_scan_ms = now;

    const bool down = TP_Scan(raw) != 0;
    const TouchPoint point = {tp_dev.x[0], tp_dev.y[0]};

    if (down)
    {
        /* 手指停在原地时，屏幕无需重复处理同一个位置。 */
        if (was_down && point.x == last_point.x && point.y == last_point.y)
        {
            return false;
        }
        event->type = was_down ? TOUCH_MOVE : TOUCH_DOWN;
        event->point = point;
        last_point = point;
    }
    else if (was_down)
    {
        event->type = TOUCH_UP;
        event->point = last_point;
    }
    else
    {
        return false;
    }

    was_down = down;
    return true;
}
