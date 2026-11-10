#ifndef SECOND_TASK_MENU_H
#define SECOND_TASK_MENU_H

#include "touch_input.h"

/* 菜单返回的选项由 app.c 转成实际的功能页面。 */
typedef enum
{
    MENU_NONE,
    MENU_PWM,
    MENU_STOPWATCH,
    MENU_CAPTURE,
    MENU_DUAL_PWM,
    MENU_SWEEP,
    MENU_SPWM,
    MENU_SNAKE
} MenuItem;

/* 绘制七项功能及返回菜单的操作提示。 */
void Menu_Show(void);

/* 返回本次按下选中的功能。点空白或滑动时返回 MENU_NONE。 */
MenuItem Menu_HandleTouch(TouchEvent event);

#endif
