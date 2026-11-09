#ifndef SECOND_TASK_UI_H
#define SECOND_TASK_UI_H

#include "capture.h"
#include "stopwatch.h"
#include "touch_input.h"

typedef struct
{
    uint32_t frequency_hz;
    uint8_t duty_percent;
} PwmSettings;

typedef enum
{
    UI_NO_CHANGE,
    UI_SET_FREQUENCY,
    UI_SET_DUTY
} UiAction;

typedef enum
{
    UI_STOPWATCH_NONE,
    UI_STOPWATCH_TOGGLE,
    UI_STOPWATCH_LAP,
    UI_STOPWATCH_RESET
} UiStopwatchAction;

void Ui_ShowPwm(PwmSettings settings);
void Ui_ShowCapture(const CaptureResult *result);
void Ui_ShowStopwatch(uint32_t elapsed_ms, bool running,
                      const StopwatchLap laps[STOPWATCH_LAP_CAPACITY]);
UiStopwatchAction Ui_HandleStopwatchTouch(TouchEvent event);
UiAction Ui_HandleTouch(TouchEvent event, PwmSettings settings, uint32_t *value);
void Ui_ShowCalibration(const TouchPoint targets[4], bool retry);
void Ui_RemoveCalibrationTarget(TouchPoint target);

#endif
