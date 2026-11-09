#include "app.h"

#include "app_config.h"
#include "capture.h"
#include "main.h"
#include "pwm.h"
#include "stopwatch.h"
#include "tftlcd.h"
#include "touch.h"
#include "touch_input.h"
#include "ui.h"

#define KEY0_DEBOUNCE_MS 20U

typedef enum
{
    PAGE_PWM,
    PAGE_STOPWATCH,
    PAGE_CAPTURE
} AppPage;

/* 开机占空比以 50% 为基准，按步进取整，再限制在上限内。 */
enum
{
    STARTUP_DUTY_ROUNDED =
        ((50U + PWM_DUTY_STEP_PERCENT / 2U) / PWM_DUTY_STEP_PERCENT) * PWM_DUTY_STEP_PERCENT,
    STARTUP_DUTY_PERCENT =
        STARTUP_DUTY_ROUNDED <= PWM_DUTY_MAX_PERCENT ? STARTUP_DUTY_ROUNDED : PWM_DUTY_MAX_PERCENT
};

static PwmSettings settings = {2000U, STARTUP_DUTY_PERCENT};
static bool calibrated;
static AppPage current_page = PAGE_PWM;
static TouchPoint calibration_targets[4];
static uint16_t calibration_samples[4][2];
static uint8_t calibration_index;
static uint32_t last_stopwatch_refresh_ms;
static uint32_t last_capture_refresh_ms;

static bool Key0_Pressed(void)
{
    static bool last_raw_down;
    static bool stable_down;
    static uint32_t last_change_ms;

    const uint32_t now = HAL_GetTick();
    const bool raw_down = HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET;
    if (raw_down != last_raw_down)
    {
        last_raw_down = raw_down;
        last_change_ms = now;
    }

    /* 按下和松开都要稳定 20 ms，只在确认按下时返回 true。 */
    if (raw_down != stable_down && now - last_change_ms >= KEY0_DEBOUNCE_MS)
    {
        stable_down = raw_down;
        return stable_down;
    }
    return false;
}

void App_Init(void)
{
    /* 先提交符合上限的开机参数，再开启 PA11 输出。 */
    Pwm_Request(settings.frequency_hz, settings.duty_percent);
    Pwm_Init();

    /* main.c 已初始化 TIM6，这里启动秒表的 1 ms 时间基准。 */
    Stopwatch_Init();

    /* TIM5 在后台持续测量 PA0，功能三读取最新结果。 */
    Capture_Init();

    TFTLCD_Init();
    calibrated = TouchInput_Init();

    if (calibrated)
    {
        Ui_ShowPwm(settings);
        return;
    }

    /* 使用与已验证的四点校准相同的目标位置。 */
    calibration_targets[0] = (TouchPoint){42, 100};
    calibration_targets[1] = (TouchPoint){tftlcd_data.width - 43, 100};
    calibration_targets[2] = (TouchPoint){42, tftlcd_data.height - 49};
    calibration_targets[3] = (TouchPoint){tftlcd_data.width - 43, tftlcd_data.height - 49};
    calibration_index = 0;
    Ui_ShowCalibration(calibration_targets, false);
}

static void StopwatchPage_Task(void)
{
    TouchEvent event;
    UiStopwatchAction action = UI_STOPWATCH_NONE;
    if (TouchInput_Poll(false, &event))
    {
        action = Ui_HandleStopwatchTouch(event);
    }

    if (action == UI_STOPWATCH_TOGGLE)
    {
        Stopwatch_ToggleRunning();
    }
    else if (action == UI_STOPWATCH_LAP)
    {
        Stopwatch_RecordLap();
    }
    else if (action == UI_STOPWATCH_RESET)
    {
        Stopwatch_Reset();
    }

    /* 按钮操作立即刷新。运行时每 40 ms 刷新，没触摸也会更新。 */
    const uint32_t now = Stopwatch_GetTickMs();
    const bool running = Stopwatch_IsRunning();
    if (action != UI_STOPWATCH_NONE || (running && now - last_stopwatch_refresh_ms >= 40U))
    {
        Ui_ShowStopwatch(Stopwatch_GetElapsedMs(), running, Stopwatch_GetLaps());
        last_stopwatch_refresh_ms = now;
    }
}

void App_Task(void)
{
    Capture_Task();

    /* KEY0 依次切到秒表、测量和 PWM。 */
    if (calibrated && Key0_Pressed())
    {
        switch (current_page)
        {
            case PAGE_PWM:
                current_page = PAGE_STOPWATCH;
                last_stopwatch_refresh_ms = Stopwatch_GetTickMs();
                Ui_ShowStopwatch(Stopwatch_GetElapsedMs(), Stopwatch_IsRunning(), Stopwatch_GetLaps());
                break;
            case PAGE_STOPWATCH:
                current_page = PAGE_CAPTURE;
                Ui_ShowCapture(Capture_GetResult());
                last_capture_refresh_ms = HAL_GetTick();
                break;
            case PAGE_CAPTURE:
                current_page = PAGE_PWM;
                Ui_ShowPwm(settings);
                break;
        }
        return;
    }

    if (current_page == PAGE_STOPWATCH)
    {
        StopwatchPage_Task();
        return;
    }

    TouchEvent event;
    if (current_page == PAGE_CAPTURE)
    {
        /* 刷新与触摸无关。保留触摸采样，以便切页后正确识别松手。 */
        TouchInput_Poll(false, &event);
        const uint32_t now = HAL_GetTick();
        if (now - last_capture_refresh_ms >= 50U)
        {
            Ui_ShowCapture(Capture_GetResult());
            last_capture_refresh_ms = now;
        }
        return;
    }

    if (!TouchInput_Poll(!calibrated, &event))
    {
        return;
    }

    if (!calibrated)
    {
        if (event.type == TOUCH_UP)
        {
            calibration_samples[calibration_index][0] = event.point.x;
            calibration_samples[calibration_index][1] = event.point.y;
            Ui_RemoveCalibrationTarget(calibration_targets[calibration_index]);
            ++calibration_index;

            if (calibration_index == 4U)
            {
                calibrated = TP_CalibrateFromPoints(calibration_samples,
                    calibration_targets[0].x, calibration_targets[0].y,
                    calibration_targets[3].x, calibration_targets[3].y) != 0;

                if (calibrated)
                {
                    Ui_ShowPwm(settings);
                }
                else
                {
                    calibration_index = 0;
                    Ui_ShowCalibration(calibration_targets, true);
                }
            }
        }
        return;
    }

    /* 键盘确认或滑块跨档时，界面通过 &value 写入新值。 */
    uint32_t value;
    const UiAction action = Ui_HandleTouch(event, settings, &value);
    if (action == UI_NO_CHANGE)
    {
        return;
    }

    /* 只替换本次操作修改的那一项，另一项沿用当前值。 */
    if (action == UI_SET_FREQUENCY)
    {
        settings.frequency_hz = value;
    }
    else
    {
        settings.duty_percent = (uint8_t)value;
    }

    /* 第一个参数是频率（Hz），第二个是占空比（30 表示 30%）。 */
    Pwm_Request(settings.frequency_hz, settings.duty_percent);
    Ui_ShowPwm(settings);
}
