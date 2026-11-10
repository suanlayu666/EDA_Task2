
#include "ui.h"

#include "app_config.h"

#include <stdio.h>
#include <string.h>
#include "tftlcd.h"

#define PAGE_BG 0xEF7DU
#define HEADER_BG 0x1949U
#define ACCENT 0x1479U
#define TRACK_BG 0xC638U

enum
{
    SLIDER_LEFT = 28,
    SLIDER_RIGHT = 211,
    SLIDER_Y = 222,
    KNOB_RADIUS = 10,
    KEY_LEFT = 14,
    KEY_TOP = 148,
    KEY_STEP_X = 72,
    KEY_STEP_Y = 34,
    KEY_WIDTH = 66,
    KEY_HEIGHT = 29,
    KEY_COLUMNS = 3,
    KEY_ROWS = 4,
    STOPWATCH_TOGGLE_X = 14,
    STOPWATCH_LAP_X = 87,
    STOPWATCH_RESET_X = 160,
    STOPWATCH_BUTTON_Y = 150,
    STOPWATCH_BUTTON_WIDTH = 66,
    STOPWATCH_BUTTON_HEIGHT = 44,
    STOPWATCH_TIME_TEXT_SIZE = 13,
    DUAL_A_X = 14,
    DUAL_B_X = 126,
    DUAL_VALUE_WIDTH = 100,
    DUAL_VALUE_HEIGHT = 36,
    DUAL_FREQUENCY_Y = 82,
    DUAL_DUTY_Y = 133,
    DUAL_PHASE_DOWN_X = 14,
    DUAL_SYNC_X = 87,
    DUAL_PHASE_UP_X = 160,
    DUAL_CONTROL_Y = 226,
    DUAL_CONTROL_WIDTH = 66,
    DUAL_CONTROL_HEIGHT = 36
};

typedef enum
{
    EDIT_NONE,
    EDIT_FREQUENCY,
    EDIT_DUTY
} EditField;

static const char *const key_labels[] =
{
    "1", "2", "3",
    "4", "5", "6",
    "7", "8", "9",
    "DEL", "0", "OK"
};

static EditField edit_field;
static bool edit_channel_b;
static bool main_visible;
static bool dragging;
static int drag_offset;
static uint32_t edit_value;
static unsigned int edit_digits;
static PwmSettings drawn_settings;
static bool dual_pwm_visible;
static DualPwmSettings drawn_dual_pwm;
static bool stopwatch_visible;
static bool drawn_stopwatch_running;
static uint32_t drawn_stopwatch_ms;
static uint32_t drawn_lap_number;
static bool capture_visible;
static bool drawn_capture_valid;
static char drawn_capture_frequency[18];
static char drawn_capture_duty[18];

typedef enum
{
    HEADER_PWM,
    HEADER_STOPWATCH,
    HEADER_CAPTURE,
    HEADER_DUAL_PWM
} PageHeader;

/* “功、能、一、二、三、四”的固定字模，由现有 LCD 绘图接口显示。 */
static const uint32_t title_bits[6][24] =
{
    {
        0x000100U, 0x000300U, 0x000300U, 0x7FC300U, 0x7FC300U, 0x061FFCU,
        0x063FFCU, 0x06030CU, 0x06030CU, 0x06030CU, 0x06030CU, 0x06030CU,
        0x06030CU, 0x06060CU, 0x06060CU, 0x07C60CU, 0x1FCE0CU, 0x7E0C0CU,
        0x601C0CU, 0x00380CU, 0x0030F8U, 0x0061F8U, 0x000000U, 0x000000U,
    },
    {
        0x000000U, 0x060300U, 0x0C4300U, 0x0C63FCU, 0x1863FCU, 0x30F300U,
        0x7FFB00U, 0x301B00U, 0x000300U, 0x3FF3FCU, 0x3FF3FCU, 0x303000U,
        0x303300U, 0x3FF300U, 0x3033FCU, 0x3033FCU, 0x3FF300U, 0x3FF300U,
        0x303306U, 0x303306U, 0x3073FCU, 0x31E3FCU, 0x000000U, 0x000000U,
    },
    {
        0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U,
        0x000000U, 0x7FFFFEU, 0x7FFFFEU, 0x000000U, 0x000000U, 0x000000U,
        0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U,
        0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U,
    },
    {
        0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U,
        0x1FFFF8U, 0x1FFFF8U, 0x000000U, 0x000000U, 0x000000U, 0x000000U,
        0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x7FFFFEU,
        0x7FFFFEU, 0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U,
    },
    {
        0x000000U, 0x000000U, 0x000000U, 0x1FFFF8U, 0x1FFFF8U, 0x000000U,
        0x000000U, 0x000000U, 0x000000U, 0x0FFFF0U, 0x0FFFF0U, 0x000000U,
        0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x7FFFFEU,
        0x7FFFFEU, 0x000000U, 0x000000U, 0x000000U, 0x000000U, 0x000000U,
    },
    {
        0x000000U, 0x000000U, 0x000000U, 0x3FFFFCU, 0x3FFFFCU, 0x30C60CU,
        0x30C60CU, 0x30C60CU, 0x30C60CU, 0x30C60CU, 0x31860CU, 0x31860CU,
        0x3307ECU, 0x3307ECU, 0x30000CU, 0x30000CU, 0x30000CU, 0x30000CU,
        0x30000CU, 0x3FFFFCU, 0x3FFFFCU, 0x000000U, 0x000000U, 0x000000U,
    },
};

static void DrawText(uint16_t x, uint16_t y, uint16_t width, uint8_t size,
                     const char *text, uint16_t color, uint16_t background)
{
    FRONT_COLOR = color;
    BACK_COLOR = background;
    LCD_ShowString(x, y, width, size, size, (u8 *)text);
}

static void DrawHeader(PageHeader page)
{
    LCD_Fill(0, 0, tftlcd_data.width - 1, 45, HEADER_BG);
    for (unsigned int glyph = 0; glyph < 3U; ++glyph)
    {
        /* 前两个字共用，最后一个字按页面选取。 */
        const unsigned int index = glyph == 2U ? glyph + page : glyph;
        for (unsigned int y = 0; y < 24U; ++y)
        {
            const uint32_t row = title_bits[index][y];
            for (unsigned int x = 0; x < 24U; ++x)
            {
                if (row & (1UL << (23U - x)))
                {
                    LCD_DrawFRONT_COLOR(14U + glyph * 26U + x, 11U + y, WHITE);
                }
            }
        }
    }
    static const char *const labels[] = {"PWM", "STOPWATCH", "CAPTURE", "DUAL PWM"};
    DrawText(153, 16, 72, 16, labels[page], WHITE, HEADER_BG);
}

static int KnobPosition(uint8_t duty_percent)
{
    const unsigned int width = SLIDER_RIGHT - SLIDER_LEFT;
    return SLIDER_LEFT + (width * duty_percent + PWM_DUTY_MAX_PERCENT / 2U) / PWM_DUTY_MAX_PERCENT;
}

static void DrawSlider(uint8_t duty_percent)
{
    const int knob = KnobPosition(duty_percent);
    LCD_Fill(15, SLIDER_Y - 13, 224, SLIDER_Y + 13, PAGE_BG);
    LCD_Fill(SLIDER_LEFT, SLIDER_Y - 2, SLIDER_RIGHT, SLIDER_Y + 2, TRACK_BG);
    LCD_Fill(SLIDER_LEFT, SLIDER_Y - 2, knob, SLIDER_Y + 2, ACCENT);
    LCD_Fill(knob - KNOB_RADIUS, SLIDER_Y - KNOB_RADIUS,
             knob + KNOB_RADIUS, SLIDER_Y + KNOB_RADIUS, ACCENT);
}

void Ui_ShowPwm(PwmSettings settings)
{
    dual_pwm_visible = false;
    capture_visible = false;
    stopwatch_visible = false;
    const bool full_redraw = !main_visible;
    if (full_redraw)
    {
        edit_field = EDIT_NONE;
        dragging = false;
        LCD_Clear(PAGE_BG);
        DrawHeader(HEADER_PWM);
        DrawText(14, 56, 210, 16, "Tap a value to edit", BLACK, PAGE_BG);
        LCD_Fill(14, 83, 225, 123, WHITE);
        LCD_Fill(14, 135, 225, 175, WHITE);
        DrawText(24, 95, 72, 16, "Freq", BLACK, WHITE);
        DrawText(24, 147, 72, 16, "Duty", BLACK, WHITE);
        DrawText(14, 189, 210, 16, "Drag to adjust duty", BLACK, PAGE_BG);
        DrawText(23, 243, 48, 16, "0%", BLACK, PAGE_BG);
        char max_duty_label[5];
        (void)snprintf(max_duty_label, sizeof max_duty_label, "%u%%", (unsigned int)PWM_DUTY_MAX_PERCENT);
        DrawText(183, 243, 44, 16, max_duty_label, BLACK, PAGE_BG);
        DrawText(14, 285, 210, 16, "PWM OUT PA11", HEADER_BG, PAGE_BG);
    }

    char text[18];
    if (full_redraw || settings.frequency_hz != drawn_settings.frequency_hz)
    {
        LCD_Fill(96, 88, 220, 120, WHITE);
        (void)snprintf(text, sizeof text, "%lu Hz", (unsigned long)settings.frequency_hz);
        DrawText(101, 92, 120, 24, text, ACCENT, WHITE);
    }
    if (full_redraw || settings.duty_percent != drawn_settings.duty_percent)
    {
        LCD_Fill(96, 140, 220, 172, WHITE);
        (void)snprintf(text, sizeof text, "%u %%", (unsigned int)settings.duty_percent);
        DrawText(101, 144, 120, 24, text, ACCENT, WHITE);
        DrawSlider(settings.duty_percent);
    }

    drawn_settings = settings;
    main_visible = true;
}

void Ui_RefreshPwm(PwmSettings settings)
{
    if (main_visible)
    {
        Ui_ShowPwm(settings);
    }
}

static void DrawButton(int x, int y, int width, int height,
                       const char *label, uint16_t background, uint16_t color)
{
    LCD_Fill(x, y, x + width - 1, y + height - 1, background);
    const int text_x = x + (width - (int)strlen(label) * 8) / 2;
    DrawText(text_x, y + (height - 16) / 2, width, 16, label, color, background);
}

static void DrawEntry(bool invalid)
{
    char text[18] = "_";
    if (edit_digits > 0U)
    {
        (void)snprintf(text, sizeof text, "%0*lu", (int)edit_digits, (unsigned long)edit_value);
    }
    LCD_Fill(14, 104, 225, 137, WHITE);
    DrawText(23, 113, 40, 16, "New", BLACK, WHITE);
    DrawText(77, 109, 138, 24, text, ACCENT, WHITE);

    LCD_Fill(85, 291, 225, 316, PAGE_BG);
    if (invalid)
    {
        const char *hint = "Use 1-200000 Hz";
        if (edit_field == EDIT_DUTY)
        {
            (void)snprintf(text, sizeof text, "Step %u%%, 0-%u%%",
                           (unsigned int)PWM_DUTY_STEP_PERCENT, (unsigned int)PWM_DUTY_MAX_PERCENT);
            hint = text;
        }
        DrawText(86, 296, 137, 16, hint, RED, PAGE_BG);
    }
}

static void OpenKeyboard(EditField field, PwmSettings settings, PageHeader page, bool channel_b)
{
    edit_field = field;
    edit_channel_b = channel_b;
    edit_value = 0;
    edit_digits = 0;
    main_visible = false;
    dual_pwm_visible = false;

    LCD_Clear(PAGE_BG);
    DrawHeader(page);
    const char *prompt = field == EDIT_FREQUENCY ? "Edit frequency" : "Edit duty";
    char text[28];
    if (page == HEADER_DUAL_PWM)
    {
        (void)snprintf(text, sizeof text, "%c: %s", channel_b ? 'B' : 'A', prompt);
        prompt = text;
    }
    DrawText(14, 55, 211, 16, prompt, BLACK, PAGE_BG);

    if (field == EDIT_FREQUENCY)
    {
        (void)snprintf(text, sizeof text, "Current  %lu Hz", (unsigned long)settings.frequency_hz);
    }
    else
    {
        (void)snprintf(text, sizeof text, "Current  %u %%", (unsigned int)settings.duty_percent);
    }
    DrawText(14, 79, 211, 16, text, HEADER_BG, PAGE_BG);
    DrawEntry(false);

    for (int i = 0; i < KEY_COLUMNS * KEY_ROWS; ++i)
    {
        const bool confirm = i == 11;
        DrawButton(KEY_LEFT + (i % KEY_COLUMNS) * KEY_STEP_X,
                   KEY_TOP + (i / KEY_COLUMNS) * KEY_STEP_Y,
                   KEY_WIDTH, KEY_HEIGHT, key_labels[i],
                   confirm ? ACCENT : WHITE, confirm ? WHITE : BLACK);
    }
    DrawButton(14, 291, 66, 26, "BACK", HEADER_BG, WHITE);
}

static UiAction HandleKeyboard(TouchPoint point, uint32_t *value)
{
    if (point.x >= 14U && point.x < 80U && point.y >= 291U && point.y < 317U)
    {
        edit_field = EDIT_NONE;
        return UI_CANCEL_EDIT;
    }

    const int dx = point.x - KEY_LEFT;
    const int dy = point.y - KEY_TOP;
    if (dx < 0 || dx >= KEY_COLUMNS * KEY_STEP_X ||
        dy < 0 || dy >= KEY_ROWS * KEY_STEP_Y ||
        dx % KEY_STEP_X >= KEY_WIDTH || dy % KEY_STEP_Y >= KEY_HEIGHT)
    {
        return UI_NO_CHANGE;
    }

    const int key = (dy / KEY_STEP_Y) * KEY_COLUMNS + dx / KEY_STEP_X;
    if (key == 11)
    {
        const bool frequency = edit_field == EDIT_FREQUENCY;
        if (edit_digits == 0U || (frequency && (edit_value == 0U || edit_value > 200000U)) ||
            (!frequency && (edit_value > PWM_DUTY_MAX_PERCENT ||
                            (edit_value != PWM_DUTY_MAX_PERCENT && edit_value % PWM_DUTY_STEP_PERCENT != 0U))))
        {
            DrawEntry(true);
            return UI_NO_CHANGE;
        }

        *value = edit_value;
        edit_field = EDIT_NONE;
        return frequency ? UI_SET_FREQUENCY : UI_SET_DUTY;
    }

    if (key == 9)
    {
        if (edit_digits > 0U)
        {
            edit_value /= 10U;
            --edit_digits;
        }
    }
    else if (edit_digits < (edit_field == EDIT_FREQUENCY ? 6U : 3U))
    {
        edit_value = edit_value * 10U + (uint32_t)(key_labels[key][0] - '0');
        ++edit_digits;
    }
    DrawEntry(false);
    return UI_NO_CHANGE;
}

UiAction Ui_HandleTouch(TouchEvent event, PwmSettings settings, uint32_t *value)
{
    if (edit_field != EDIT_NONE)
    {
        if (event.type != TOUCH_DOWN)
        {
            return UI_NO_CHANGE;
        }
        const UiAction action = HandleKeyboard(event.point, value);
        if (action == UI_CANCEL_EDIT)
        {
            Ui_ShowPwm(settings);
            return UI_NO_CHANGE;
        }
        return action;
    }

    const TouchPoint point = event.point;
    if (event.type == TOUCH_DOWN)
    {
        if (point.x >= 14U && point.x <= 225U)
        {
            if (point.y >= 83U && point.y <= 123U)
            {
                OpenKeyboard(EDIT_FREQUENCY, settings, HEADER_PWM, false);
                return UI_NO_CHANGE;
            }
            if (point.y >= 135U && point.y <= 175U)
            {
                OpenKeyboard(EDIT_DUTY, settings, HEADER_PWM, false);
                return UI_NO_CHANGE;
            }
        }

        const int knob = KnobPosition(settings.duty_percent);
        const int offset = point.x - knob;
        if (offset >= -14 && offset <= 14 && point.y >= SLIDER_Y - 14 && point.y <= SLIDER_Y + 14)
        {
            /* 保存手指相对滑块中心的距离，按下时滑块不会跳动。 */
            dragging = true;
            drag_offset = offset;
        }
    }
    else if (event.type == TOUCH_UP)
    {
        dragging = false;
    }
    else if (dragging)
    {
        const int x = point.x - drag_offset;
        if (x <= SLIDER_LEFT)
        {
            *value = 0;
        }
        else if (x >= SLIDER_RIGHT)
        {
            *value = PWM_DUTY_MAX_PERCENT;
        }
        else
        {
            /* 比较手指到相邻两档的距离，最后一档使用配置的上限。 */
            const unsigned int width = SLIDER_RIGHT - SLIDER_LEFT;
            const unsigned int position = (unsigned int)(x - SLIDER_LEFT) * PWM_DUTY_MAX_PERCENT;
            const unsigned int lower = position / (width * PWM_DUTY_STEP_PERCENT) * PWM_DUTY_STEP_PERCENT;
            unsigned int upper = lower + PWM_DUTY_STEP_PERCENT;
            if (upper > PWM_DUTY_MAX_PERCENT)
            {
                upper = PWM_DUTY_MAX_PERCENT;
            }
            *value = position - lower * width < upper * width - position ? lower : upper;
        }
        if (*value != settings.duty_percent)
        {
            return UI_SET_DUTY;
        }
    }
    return UI_NO_CHANGE;
}

void Ui_ShowCalibration(const TouchPoint targets[4], bool retry)
{
    LCD_Clear(WHITE);
    DrawHeader(HEADER_PWM);
    DrawText(14, 55, 211, 16, retry ? "Retry: tap centers 1-4" : "Tap centers 1-4 in order", BLACK, WHITE);
    for (unsigned int i = 0; i < 4U; ++i)
    {
        const TouchPoint target = targets[i];
        const char label[] = {(char)('1' + i), '\0'};
        FRONT_COLOR = ACCENT;
        LCD_Draw_Circle(target.x, target.y, 17);
        DrawText(target.x - 4, target.y - 8, 8, 16, label, ACCENT, WHITE);
    }
}

void Ui_RemoveCalibrationTarget(TouchPoint target)
{
    LCD_Fill(target.x - 18, target.y - 18, target.x + 18, target.y + 18, WHITE);
}

static void FormatStopwatchTime(char text[STOPWATCH_TIME_TEXT_SIZE], uint32_t elapsed_ms)
{
    (void)snprintf(text, STOPWATCH_TIME_TEXT_SIZE, "%02lu:%02lu.%03lu",
                   (unsigned long)(elapsed_ms / 60000U),
                   (unsigned long)((elapsed_ms / 1000U) % 60U),
                   (unsigned long)(elapsed_ms % 1000U));
}

static void DrawStopwatchLaps(const StopwatchLap laps[STOPWATCH_LAP_CAPACITY])
{
    LCD_Fill(14, 230, 225, 312, PAGE_BG);
    for (unsigned int i = 0; i < STOPWATCH_LAP_CAPACITY && laps[i].number != 0U; ++i)
    {
        const uint16_t y = 234U + i * 20U;
        char text[STOPWATCH_TIME_TEXT_SIZE];
        (void)snprintf(text, sizeof text, "%lu", (unsigned long)laps[i].number);
        DrawText(14, y, 60, 12, text, BLACK, PAGE_BG);
        FormatStopwatchTime(text, laps[i].total_ms);
        DrawText(77, y, 72, 12, text, BLACK, PAGE_BG);
        FormatStopwatchTime(text, laps[i].split_ms);
        DrawText(154, y, 72, 12, text, BLACK, PAGE_BG);
    }
}

void Ui_ShowStopwatch(uint32_t elapsed_ms, bool running,
                      const StopwatchLap laps[STOPWATCH_LAP_CAPACITY])
{
    dual_pwm_visible = false;
    capture_visible = false;
    const bool entering = !stopwatch_visible;
    if (entering)
    {
        /* 离开 PWM 页时结束键盘编辑和滑块拖动。 */
        edit_field = EDIT_NONE;
        dragging = false;
        main_visible = false;

        LCD_Clear(PAGE_BG);
        DrawHeader(HEADER_STOPWATCH);
        LCD_Fill(14, 80, 225, 124, WHITE);
        DrawButton(STOPWATCH_RESET_X, STOPWATCH_BUTTON_Y, STOPWATCH_BUTTON_WIDTH,
                   STOPWATCH_BUTTON_HEIGHT, "RESET", HEADER_BG, WHITE);
        DrawText(14, 209, 48, 16, "LAP", HEADER_BG, PAGE_BG);
        DrawText(77, 209, 72, 16, "TOTAL", HEADER_BG, PAGE_BG);
        DrawText(154, 209, 72, 16, "SPLIT", HEADER_BG, PAGE_BG);
        stopwatch_visible = true;
    }

    if (entering || elapsed_ms != drawn_stopwatch_ms)
    {
        char time_text[STOPWATCH_TIME_TEXT_SIZE];
        char display[STOPWATCH_TIME_TEXT_SIZE] = "            ";
        FormatStopwatchTime(time_text, elapsed_ms);
        const size_t length = strlen(time_text);
        memcpy(display + (sizeof display - 1U - length) / 2U, time_text, length);

        /* 固定宽度覆盖旧数字，清零时较长的分钟数也不会残留。 */
        DrawText(48, 90, 144, 24, display, ACCENT, WHITE);
        drawn_stopwatch_ms = elapsed_ms;
    }
    if (entering || running != drawn_stopwatch_running)
    {
        DrawButton(STOPWATCH_TOGGLE_X, STOPWATCH_BUTTON_Y, STOPWATCH_BUTTON_WIDTH,
                   STOPWATCH_BUTTON_HEIGHT, running ? "PAUSE" : "START", ACCENT, WHITE);
        DrawButton(STOPWATCH_LAP_X, STOPWATCH_BUTTON_Y, STOPWATCH_BUTTON_WIDTH,
                   STOPWATCH_BUTTON_HEIGHT, "LAP", running ? HEADER_BG : TRACK_BG,
                   running ? WHITE : BLACK);
        drawn_stopwatch_running = running;
    }
    /* 记录区只在记圈、清零或切入页面时刷新。 */
    if (entering || drawn_lap_number != laps[0].number)
    {
        DrawStopwatchLaps(laps);
        drawn_lap_number = laps[0].number;
    }
}

UiStopwatchAction Ui_HandleStopwatchTouch(TouchEvent event)
{
    const TouchPoint point = event.point;
    if (event.type != TOUCH_DOWN || point.y < STOPWATCH_BUTTON_Y ||
        point.y >= STOPWATCH_BUTTON_Y + STOPWATCH_BUTTON_HEIGHT)
    {
        return UI_STOPWATCH_NONE;
    }

    if (point.x >= STOPWATCH_TOGGLE_X && point.x < STOPWATCH_TOGGLE_X + STOPWATCH_BUTTON_WIDTH)
    {
        return UI_STOPWATCH_TOGGLE;
    }
    if (point.x >= STOPWATCH_LAP_X && point.x < STOPWATCH_LAP_X + STOPWATCH_BUTTON_WIDTH)
    {
        return UI_STOPWATCH_LAP;
    }
    if (point.x >= STOPWATCH_RESET_X && point.x < STOPWATCH_RESET_X + STOPWATCH_BUTTON_WIDTH)
    {
        return UI_STOPWATCH_RESET;
    }
    return UI_STOPWATCH_NONE;
}

static void DrawCaptureValue(uint16_t y, const char *text, char *previous, bool entering)
{
    if (entering || strcmp(text, previous) != 0)
    {
        LCD_Fill(96, y - 4, 220, y + 28, WHITE);
        const uint8_t font_size = strlen(text) <= 10U ? 24U : 16U;
        DrawText(101, y, 120, font_size, text, ACCENT, WHITE);
        strcpy(previous, text);
    }
}

void Ui_ShowCapture(const CaptureResult *result)
{
    dual_pwm_visible = false;
    const bool entering = !capture_visible;
    if (entering)
    {
        main_visible = false;
        stopwatch_visible = false;
        capture_visible = true;

        LCD_Clear(PAGE_BG);
        DrawHeader(HEADER_CAPTURE);
        DrawText(14, 56, 210, 16, "INPUT PA0", HEADER_BG, PAGE_BG);
        LCD_Fill(14, 83, 225, 123, WHITE);
        LCD_Fill(14, 135, 225, 175, WHITE);
        DrawText(24, 95, 72, 16, "Freq", BLACK, WHITE);
        DrawText(24, 147, 72, 16, "Duty", BLACK, WHITE);
    }

    char frequency[sizeof drawn_capture_frequency] = "--- Hz";
    char duty[sizeof drawn_capture_duty] = "--- %";
    if (result->valid)
    {
        /* 低频保留三位小数，方便看出 1 Hz 附近的变化。 */
        if (result->frequency_hz < 1000.0f)
        {
            const uint32_t millihz = (uint32_t)(result->frequency_hz * 1000.0f + 0.5f);
            if (millihz < 1000000U)
            {
                (void)snprintf(frequency, sizeof frequency, "%lu.%03lu Hz",
                               (unsigned long)(millihz / 1000U), (unsigned long)(millihz % 1000U));
            }
            else
            {
                strcpy(frequency, "1000 Hz");
            }
        }
        else
        {
            (void)snprintf(frequency, sizeof frequency, "%lu Hz",
                           (unsigned long)(result->frequency_hz + 0.5f));
        }

        const uint32_t duty_hundredths = (uint32_t)(result->duty_percent * 100.0f + 0.5f);
        (void)snprintf(duty, sizeof duty, "%lu.%02lu %%",
                       (unsigned long)(duty_hundredths / 100U), (unsigned long)(duty_hundredths % 100U));
    }

    /* 数值未变就保留原来的像素，信号丢失时用占位符覆盖旧读数。 */
    DrawCaptureValue(92, frequency, drawn_capture_frequency, entering);
    DrawCaptureValue(144, duty, drawn_capture_duty, entering);
    if (entering || result->valid != drawn_capture_valid)
    {
        LCD_Fill(14, 195, 225, 221, PAGE_BG);
        DrawText(14, 200, 210, 16, result->valid ? "SIGNAL OK" : "WAITING FOR SIGNAL",
                 HEADER_BG, PAGE_BG);
        drawn_capture_valid = result->valid;
    }
}

static void DrawDualPwmChannel(int x, PwmSettings settings, PwmSettings previous, bool entering)
{
    char text[18];
    if (entering || settings.frequency_hz != previous.frequency_hz)
    {
        (void)snprintf(text, sizeof text, "%lu Hz", (unsigned long)settings.frequency_hz);
        DrawButton(x, DUAL_FREQUENCY_Y, DUAL_VALUE_WIDTH, DUAL_VALUE_HEIGHT, text, WHITE, ACCENT);
    }
    if (entering || settings.duty_percent != previous.duty_percent)
    {
        (void)snprintf(text, sizeof text, "Duty %u%%", (unsigned int)settings.duty_percent);
        DrawButton(x, DUAL_DUTY_Y, DUAL_VALUE_WIDTH, DUAL_VALUE_HEIGHT, text, WHITE, ACCENT);
    }
}

static void DrawPhaseButton(int x, char sign)
{
    char text[6];
    (void)snprintf(text, sizeof text, "%c%u ", sign, (unsigned int)PWM_PHASE_STEP_DEG);
    DrawButton(x, DUAL_CONTROL_Y, DUAL_CONTROL_WIDTH, DUAL_CONTROL_HEIGHT, text, HEADER_BG, WHITE);

    /* 最后一个字符留空，用小圆代替字库里没有的度数符号。 */
    const int label_width = (int)strlen(text) * 8;
    const int degree_x = x + (DUAL_CONTROL_WIDTH + label_width) / 2 - 4;
    LCD_Draw_Circle(degree_x, DUAL_CONTROL_Y + 13, 2);
}

void Ui_ShowDualPwm(const DualPwmSettings *settings)
{
    const bool entering = !dual_pwm_visible;
    if (entering)
    {
        main_visible = false;
        stopwatch_visible = false;
        capture_visible = false;
        edit_field = EDIT_NONE;
        dragging = false;
        dual_pwm_visible = true;

        LCD_Clear(PAGE_BG);
        DrawHeader(HEADER_DUAL_PWM);
        DrawText(14, 56, 100, 16, "A  PA11", HEADER_BG, PAGE_BG);
        DrawText(126, 56, 100, 16, "B  PC7", HEADER_BG, PAGE_BG);
        DrawText(14, 306, 210, 12, "Tap values to edit", HEADER_BG, PAGE_BG);

        DrawPhaseButton(DUAL_PHASE_DOWN_X, '-');
        DrawPhaseButton(DUAL_PHASE_UP_X, '+');
    }

    DrawDualPwmChannel(DUAL_A_X, settings->a, drawn_dual_pwm.a, entering);
    DrawDualPwmChannel(DUAL_B_X, settings->b, drawn_dual_pwm.b, entering);

    if (entering || settings->phase_deg != drawn_dual_pwm.phase_deg)
    {
        char text[24];
        (void)snprintf(text, sizeof text, "B phase set: %u deg", (unsigned int)settings->phase_deg);
        LCD_Fill(14, 188, 225, 215, PAGE_BG);
        DrawText(14, 193, 210, 16, text, HEADER_BG, PAGE_BG);
    }

    if (entering || settings->sync != drawn_dual_pwm.sync)
    {
        const bool synced = settings->sync == DUAL_PWM_SYNC_ON;
        DrawButton(DUAL_SYNC_X, DUAL_CONTROL_Y, DUAL_CONTROL_WIDTH, DUAL_CONTROL_HEIGHT,
                   "SYNC", synced ? ACCENT : HEADER_BG, WHITE);
        LCD_Fill(14, 270, 225, 294, PAGE_BG);
        static const char *const status[] = {
            [DUAL_PWM_SYNC_OFF] = "SYNC OFF",
            [DUAL_PWM_SYNC_ON] = "SYNC ON",
            [DUAL_PWM_SYNC_STARTING] = "SYNCING",
            [DUAL_PWM_SYNC_STOPPING] = "LEAVING SYNC",
            [DUAL_PWM_SYNC_NEEDS_MATCH] = "SET SAME FREQ",
        };
        DrawText(14, 275, 210, 16, status[settings->sync], HEADER_BG, PAGE_BG);
    }

    drawn_dual_pwm = *settings;
}

void Ui_RefreshDualPwm(const DualPwmSettings *settings)
{
    /* 运行状态变化时刷新主页，用户正在输入的数字留在键盘上。 */
    if (dual_pwm_visible)
    {
        Ui_ShowDualPwm(settings);
    }
}

UiAction Ui_HandleDualPwmTouch(TouchEvent event, const DualPwmSettings *settings, uint32_t *value)
{
    if (event.type != TOUCH_DOWN)
    {
        return UI_NO_CHANGE;
    }

    const TouchPoint point = event.point;
    if (edit_field != EDIT_NONE)
    {
        const UiAction action = HandleKeyboard(point, value);
        if (edit_channel_b && action == UI_SET_FREQUENCY)
        {
            return UI_SET_B_FREQUENCY;
        }
        if (edit_channel_b && action == UI_SET_DUTY)
        {
            return UI_SET_B_DUTY;
        }
        return action;
    }

    if (point.y >= DUAL_CONTROL_Y && point.y < DUAL_CONTROL_Y + DUAL_CONTROL_HEIGHT)
    {
        if (point.x >= DUAL_PHASE_DOWN_X && point.x < DUAL_PHASE_DOWN_X + DUAL_CONTROL_WIDTH)
        {
            return UI_PHASE_DOWN;
        }
        if (point.x >= DUAL_SYNC_X && point.x < DUAL_SYNC_X + DUAL_CONTROL_WIDTH)
        {
            return UI_TOGGLE_SYNC;
        }
        if (point.x >= DUAL_PHASE_UP_X && point.x < DUAL_PHASE_UP_X + DUAL_CONTROL_WIDTH)
        {
            return UI_PHASE_UP;
        }
        return UI_NO_CHANGE;
    }

    const bool channel_a = point.x >= DUAL_A_X && point.x < DUAL_A_X + DUAL_VALUE_WIDTH;
    const bool channel_b = point.x >= DUAL_B_X && point.x < DUAL_B_X + DUAL_VALUE_WIDTH;
    if (channel_a || channel_b)
    {
        const PwmSettings channel = channel_b ? settings->b : settings->a;
        if (point.y >= DUAL_FREQUENCY_Y && point.y < DUAL_FREQUENCY_Y + DUAL_VALUE_HEIGHT)
        {
            OpenKeyboard(EDIT_FREQUENCY, channel, HEADER_DUAL_PWM, channel_b);
        }
        else if (point.y >= DUAL_DUTY_Y && point.y < DUAL_DUTY_Y + DUAL_VALUE_HEIGHT)
        {
            OpenKeyboard(EDIT_DUTY, channel, HEADER_DUAL_PWM, channel_b);
        }
    }
    return UI_NO_CHANGE;
}

enum
{
    SNAKE_BOARD_X = 40,
    SNAKE_BOARD_Y = 54,
    SNAKE_CELL_PIXELS = 16
};

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    const char *label;
    UiSnakeAction action;
} SnakeButton;

static const SnakeButton snake_buttons[] =
{
    { 88, 250, 64, 30, "UP", UI_SNAKE_UP },
    { 14, 285, 64, 31, "LEFT", UI_SNAKE_LEFT },
    { 88, 285, 64, 31, "DOWN", UI_SNAKE_DOWN },
    { 162, 285, 64, 31, "RIGHT", UI_SNAKE_RIGHT },
    { 106, 4, 64, 26, "AUTO", UI_SNAKE_TOGGLE_AUTO },
    { 177, 4, 56, 26, "RESET", UI_SNAKE_RESET }
};

void Ui_ShowSnake(const SnakeState *game, bool entering)
{
    static SnakeState drawn_game;
    static const uint16_t cell_colors[] = {0x2104U, 0x04A0U, 0x07E0U, 0xF986U};
    static const char *const status_labels[] = {"Press a direction", "PLAYING", "GAME OVER", "WIN"};

    if (entering)
    {
        main_visible = false;
        dual_pwm_visible = false;
        stopwatch_visible = false;
        capture_visible = false;
        edit_field = EDIT_NONE;
        dragging = false;

        LCD_Clear(PAGE_BG);
        LCD_Fill(0, 0, tftlcd_data.width - 1, 34, HEADER_BG);
        DrawText(12, 10, 80, 16, "SNAKE", WHITE, HEADER_BG);
        LCD_Fill(SNAKE_BOARD_X - 1, SNAKE_BOARD_Y - 1,
                 SNAKE_BOARD_X + SNAKE_COLUMNS * SNAKE_CELL_PIXELS - 1,
                 SNAKE_BOARD_Y + SNAKE_ROWS * SNAKE_CELL_PIXELS - 1, 0x39E7U);
    }

    for (unsigned int i = 0; i < sizeof snake_buttons / sizeof snake_buttons[0]; ++i)
    {
        const SnakeButton *button = &snake_buttons[i];
        if (entering || (button->action == UI_SNAKE_TOGGLE_AUTO && game->automatic != drawn_game.automatic))
        {
            const char *label = button->action == UI_SNAKE_TOGGLE_AUTO && game->automatic
                ? "MANUAL" : button->label;
            DrawButton(button->x, button->y, button->width, button->height, label, ACCENT, WHITE);
        }
    }

    if (entering || game->score != drawn_game.score || game->status != drawn_game.status)
    {
        char score[16];
        (void)snprintf(score, sizeof score, "Score %u", (unsigned int)game->score);
        LCD_Fill(12, 38, 227, 50, PAGE_BG);
        DrawText(12, 38, 78, 12, score, BLACK, PAGE_BG);
        DrawText(124, 38, 104, 12, status_labels[game->status], HEADER_BG, PAGE_BG);
    }

    /* 走格时只重画内容变化的格子，保留网格线和其他控件。 */
    for (unsigned int i = 0; i < SNAKE_CELL_COUNT; ++i)
    {
        if (entering || game->cells[i] != drawn_game.cells[i])
        {
            const uint16_t x = SNAKE_BOARD_X + (i % SNAKE_COLUMNS) * SNAKE_CELL_PIXELS;
            const uint16_t y = SNAKE_BOARD_Y + (i / SNAKE_COLUMNS) * SNAKE_CELL_PIXELS;
            LCD_Fill(x, y, x + SNAKE_CELL_PIXELS - 2, y + SNAKE_CELL_PIXELS - 2,
                     cell_colors[game->cells[i]]);
        }
    }
    drawn_game = *game;
}

UiSnakeAction Ui_HandleSnakeTouch(TouchEvent event)
{
    if (event.type != TOUCH_DOWN)
    {
        return UI_SNAKE_NONE;
    }

    for (unsigned int i = 0; i < sizeof snake_buttons / sizeof snake_buttons[0]; ++i)
    {
        const SnakeButton *button = &snake_buttons[i];
        if (event.point.x >= button->x && event.point.x < button->x + button->width &&
            event.point.y >= button->y && event.point.y < button->y + button->height)
        {
            return button->action;
        }
    }
    return UI_SNAKE_NONE;
}
