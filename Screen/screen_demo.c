#include "screen_demo.h"

#include <stdbool.h>
#include "tftlcd.h"
#include "touch.h"

enum
{
    SCAN_INTERVAL_MS = 15,
    BALL_RADIUS = 17,
    SLIDER_RADIUS = 14,
    SLIDER_MARGIN = 32,
    SLIDER_END_TOLERANCE = 4,
};

typedef struct
{
    int x;
    int y;
} Point;

typedef enum
{
    TEST_CALIBRATION,
    TEST_BALLS,
    TEST_SLIDER,
    TEST_PASSED,
} TestStage;

static Point balls[4];
static const uint16_t ball_colors[] = {BLUE, RED, GREEN, MAGENTA};
static uint8_t remaining_balls;
static u16 calibration_samples[4][2];
static uint8_t calibration_index;
static TestStage stage;
static bool previous_down;
static uint32_t last_scan_ms;

static int slider_x;
static int slider_left;
static int slider_right;
static int slider_y;
static int drag_offset;
static bool dragging;

static void DrawDisc(Point center, int radius, uint16_t color)
{
    for (int dy = -radius; dy <= radius; ++dy)
    {
        int dx = radius;
        while (dx * dx + dy * dy > radius * radius)
        {
            --dx;
        }
        LCD_Fill(center.x - dx, center.y + dy,
                 center.x + dx, center.y + dy, color);
    }
}

static bool HitsDisc(Point position, Point center, int radius)
{
    const int dx = position.x - center.x;
    const int dy = position.y - center.y;
    return dx * dx + dy * dy <= radius * radius;
}

static void ShowPage(u8 *message)
{
    LCD_Clear(WHITE);
    FRONT_COLOR = BLACK;
    BACK_COLOR = WHITE;
    LCD_ShowString(12, 12, 210, 24, 24, (u8 *)"TOUCH TEST");
    LCD_ShowString(12, 44, 210, 20, 16, message);
}

static void ShowBalls(u8 *message)
{
    ShowPage(message);
    for (unsigned int i = 0; i < sizeof balls / sizeof balls[0]; ++i)
    {
        DrawDisc(balls[i], BALL_RADIUS, ball_colors[i]);
        if (stage == TEST_CALIBRATION)
        {
            FRONT_COLOR = WHITE;
            LCD_ShowChar(balls[i].x - 4, balls[i].y - 8, '1' + i, 16, 1);
        }
    }
    FRONT_COLOR = BLACK;
}

static void DrawSlider(void)
{
    LCD_Fill(slider_left, slider_y - 2, slider_right, slider_y + 2, GRAY);
    LCD_Draw_Circle(slider_right, slider_y, SLIDER_RADIUS);
    DrawDisc((Point){slider_x, slider_y}, SLIDER_RADIUS, BLUE);
}

static void MoveSlider(int x)
{
    if (x == slider_x)
    {
        return;
    }

    DrawDisc((Point){slider_x, slider_y}, SLIDER_RADIUS, WHITE);
    slider_x = x;
    DrawSlider();
}

static void StartSlider(void)
{
    stage = TEST_SLIDER;
    slider_left = SLIDER_MARGIN;
    slider_right = tftlcd_data.width - 1 - SLIDER_MARGIN;
    slider_y = tftlcd_data.height / 2;
    slider_x = slider_left;
    dragging = false;

    ShowPage((u8 *)"Drag from left to right");
    DrawSlider();
}

static void RecordCalibrationTap(Point position)
{
    calibration_samples[calibration_index][0] = position.x;
    calibration_samples[calibration_index][1] = position.y;
    DrawDisc(balls[calibration_index], BALL_RADIUS, WHITE);

    if (++calibration_index == 4U)
    {
        if (TP_CalibrateFromPoints(calibration_samples,
                                   balls[0].x, balls[0].y, balls[3].x, balls[3].y))
        {
            StartSlider();
        }
        else
        {
            calibration_index = 0;
            ShowBalls((u8 *)"Retry: tap centers 1-4");
        }
    }
}

static void TapBall(Point position)
{
    for (unsigned int i = 0; i < sizeof balls / sizeof balls[0]; ++i)
    {
        const uint8_t bit = 1U << i;
        if ((remaining_balls & bit) && HitsDisc(position, balls[i], BALL_RADIUS))
        {
            remaining_balls &= (uint8_t)~bit;
            if (remaining_balls == 0U)
            {
                StartSlider();
            }
            else
            {
                DrawDisc(balls[i], BALL_RADIUS, WHITE);
            }
            return;
        }
    }
}

static void HandleSlider(Point position, bool down, bool pressed)
{
    if (!down)
    {
        if (dragging)
        {
            dragging = false;
            MoveSlider(slider_left);
        }
        return;
    }

    if (pressed)
    {
        dragging = HitsDisc(position, (Point){slider_x, slider_y}, SLIDER_RADIUS + 3);
        drag_offset = position.x - slider_x;
    }
    if (!dragging)
    {
        return;
    }

    const int x = position.x - drag_offset;
    if (x >= slider_right - SLIDER_END_TOLERANCE)
    {
        stage = TEST_PASSED;
        ShowPage((u8 *)"TEST PASSED");
        return;
    }
    MoveSlider(x < slider_left ? slider_left : x);
}

void ScreenDemo_Init(void)
{
    TFTLCD_Init();
    previous_down = false;
    last_scan_ms = HAL_GetTick();

    const bool calibrated = TP_InitWithoutCalibration() == 0;
    stage = calibrated ? TEST_BALLS : TEST_CALIBRATION;
    remaining_balls = 0x0F;
    calibration_index = 0;
    balls[0] = (Point){42, 100};
    balls[1] = (Point){tftlcd_data.width - 43, 100};
    balls[2] = (Point){42, tftlcd_data.height - 49};
    balls[3] = (Point){tftlcd_data.width - 43, tftlcd_data.height - 49};

    ShowBalls(calibrated ? (u8 *)"Tap all four balls" : (u8 *)"Tap centers 1-4 in order");
}

void ScreenDemo_Task(void)
{
    const uint32_t now = HAL_GetTick();
    if (stage == TEST_PASSED ||
        now - last_scan_ms < SCAN_INTERVAL_MS)
    {
        return;
    }
    last_scan_ms = now;

    const bool down = TP_Scan(stage == TEST_CALIBRATION) != 0;
    const bool released = !down && previous_down;
    const bool pressed = down && !previous_down;
    const Point position = {tp_dev.x[0], tp_dev.y[0]};
    previous_down = down;

    if (stage == TEST_CALIBRATION && released)
    {
        RecordCalibrationTap(position);
    }
    else if (stage == TEST_BALLS && pressed)
    {
        TapBall(position);
    }
    else if (stage == TEST_SLIDER)
    {
        HandleSlider(position, down, pressed);
    }
}
