#include "menu.h"

#include "tftlcd.h"

enum
{
    MENU_LEFT = 14,
    MENU_RIGHT = 225,
    MENU_TOP = 51,
    MENU_ROW_HEIGHT = 26,
    MENU_ROW_SPACING = 31
};

#define MENU_BACKGROUND 0xEF7DU
#define MENU_HEADER 0x1949U
#define MENU_ACCENT 0x1479U

/* 绘制和触摸共用这张表，顺序就是屏幕上的排列顺序。 */
static const struct
{
    MenuItem item;
    const char *label;
} menu_entries[] = {
    {MENU_PWM, "PWM Output"},
    {MENU_STOPWATCH, "Stopwatch"},
    {MENU_CAPTURE, "Input Capture"},
    {MENU_DUAL_PWM, "Dual PWM Phase"},
    {MENU_SWEEP, "Auto Sweep"},
    {MENU_SPWM, "SPWM Output"},
    {MENU_SNAKE, "Snake"},
    {MENU_TILT_MAZE, "Tilt Maze"},
};

/* 使用板载 LCD 字库在指定区域绘制一行菜单文字。 */
static void DrawMenuText(uint16_t x, uint16_t y, uint16_t width, uint8_t size,
                         const char *text, uint16_t color, uint16_t background)
{
    FRONT_COLOR = color;
    BACK_COLOR = background;
    LCD_ShowString(x, y, width, size, size, (u8 *)text);
}

/* 整页只在打开菜单时绘制，八个选项从上到下各占一行。 */
void Menu_Show(void)
{
    LCD_Clear(MENU_BACKGROUND);
    LCD_Fill(0, 0, tftlcd_data.width - 1U, 41, MENU_HEADER);
    DrawMenuText(18, 9, 204, 24, "FUNCTION MENU", WHITE, MENU_HEADER);

    for (unsigned int i = 0; i < sizeof menu_entries / sizeof menu_entries[0]; ++i)
    {
        const uint16_t y = MENU_TOP + i * MENU_ROW_SPACING;
        const char number[] = {(char)('1' + i), '\0'};
        LCD_Fill(MENU_LEFT, y, MENU_RIGHT, y + MENU_ROW_HEIGHT - 1U, WHITE);
        LCD_Fill(21, y + 3U, 43, y + 22U, MENU_ACCENT);
        DrawMenuText(28, y + 5U, 8, 16, number, WHITE, MENU_ACCENT);
        DrawMenuText(54, y + 5U, 160, 16, menu_entries[i].label, BLACK, WHITE);
    }
    DrawMenuText(14, 302, 210, 12, "KEY0 returns to this menu", MENU_HEADER, MENU_BACKGROUND);
}

/* 根据按下坐标查找选项。每行之间的空白不会触发切页。 */
MenuItem Menu_HandleTouch(TouchEvent event)
{
    const TouchPoint point = event.point;
    if (event.type != TOUCH_DOWN || point.x < MENU_LEFT || point.x > MENU_RIGHT)
    {
        return MENU_NONE;
    }

    for (unsigned int i = 0; i < sizeof menu_entries / sizeof menu_entries[0]; ++i)
    {
        const uint16_t y = MENU_TOP + i * MENU_ROW_SPACING;
        if (point.y >= y && point.y < y + MENU_ROW_HEIGHT)
        {
            return menu_entries[i].item;
        }
    }
    return MENU_NONE;
}
