#ifndef SECOND_TASK_UI_H
#define SECOND_TASK_UI_H

#include "capture.h"
#include "snake.h"
#include "stopwatch.h"
#include "touch_input.h"

typedef struct
{
    uint32_t frequency_hz;
    uint8_t duty_percent;
} PwmSettings;

typedef enum
{
    DUAL_PWM_SYNC_OFF,
    DUAL_PWM_SYNC_ON,
    DUAL_PWM_SYNC_STARTING,
    DUAL_PWM_SYNC_STOPPING,
    DUAL_PWM_SYNC_NEEDS_MATCH
} DualPwmSync;

typedef struct
{
    PwmSettings a;
    PwmSettings b;
    uint16_t phase_deg;
    DualPwmSync sync;
} DualPwmSettings;

typedef enum
{
    UI_NO_CHANGE,
    UI_SET_FREQUENCY,
    UI_SET_DUTY,
    UI_CANCEL_EDIT,
    UI_SET_B_FREQUENCY,
    UI_SET_B_DUTY,
    UI_TOGGLE_SYNC,
    UI_PHASE_DOWN,
    UI_PHASE_UP
} UiAction;

typedef enum
{
    UI_STOPWATCH_NONE,
    UI_STOPWATCH_TOGGLE,
    UI_STOPWATCH_LAP,
    UI_STOPWATCH_RESET
} UiStopwatchAction;

typedef enum
{
    UI_SNAKE_UP = SNAKE_UP,
    UI_SNAKE_RIGHT = SNAKE_RIGHT,
    UI_SNAKE_DOWN = SNAKE_DOWN,
    UI_SNAKE_LEFT = SNAKE_LEFT,
    UI_SNAKE_TOGGLE_AUTO,
    UI_SNAKE_RESET,
    UI_SNAKE_NONE
} UiSnakeAction;

void Ui_ShowSnake(const SnakeState *game, bool entering);
UiSnakeAction Ui_HandleSnakeTouch(TouchEvent event);
void Ui_ShowPwm(PwmSettings settings);
void Ui_RefreshPwm(PwmSettings settings);
void Ui_ShowDualPwm(const DualPwmSettings *settings);
void Ui_RefreshDualPwm(const DualPwmSettings *settings);
UiAction Ui_HandleDualPwmTouch(TouchEvent event, const DualPwmSettings *settings, uint32_t *value);
void Ui_ShowCapture(const CaptureResult *result);
void Ui_ShowStopwatch(uint32_t elapsed_ms, bool running,
                      const StopwatchLap laps[STOPWATCH_LAP_CAPACITY]);
UiStopwatchAction Ui_HandleStopwatchTouch(TouchEvent event);
UiAction Ui_HandleTouch(TouchEvent event, PwmSettings settings, uint32_t *value);
void Ui_ShowCalibration(const TouchPoint targets[4], bool retry);
void Ui_RemoveCalibrationTarget(TouchPoint target);

#endif
