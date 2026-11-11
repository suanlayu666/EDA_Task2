#include "app.h"

#include "app_config.h"
#include "capture.h"
#include "main.h"
#include "menu.h"
#include "pwm.h"
#include "snake.h"
#include "spwm.h"
#include "stopwatch.h"
#include "sweep.h"
#include "tftlcd.h"
#include "tilt_maze.h"
#include "tim.h"
#include "touch.h"
#include "touch_input.h"
#include "ui.h"

#define KEY0_DEBOUNCE_MS 20U

typedef enum
{
    PAGE_MENU,
    PAGE_PWM,
    PAGE_STOPWATCH,
    PAGE_CAPTURE,
    PAGE_DUAL_PWM,
    PAGE_SWEEP,
    PAGE_SPWM,
    PAGE_SNAKE,
    PAGE_TILT_MAZE
} AppPage;

/* 开机占空比以 50% 为基准，按步进取整，再限制在上限内。 */
enum
{
    STARTUP_DUTY_ROUNDED =
        ((50U + PWM_DUTY_STEP_PERCENT / 2U) / PWM_DUTY_STEP_PERCENT) * PWM_DUTY_STEP_PERCENT,
    STARTUP_DUTY_PERCENT =
        STARTUP_DUTY_ROUNDED <= PWM_DUTY_MAX_PERCENT ? STARTUP_DUTY_ROUNDED : PWM_DUTY_MAX_PERCENT
};

/* 功能一和功能四共用实际输出的 A 路设定。 */
static DualPwmSettings dual_pwm = {
    .a = {2000U, STARTUP_DUTY_PERCENT},
    .b = {800U, STARTUP_DUTY_PERCENT},
    .phase_deg = 0U,
    .sync = DUAL_PWM_SYNC_OFF,
};
/* 退出同步期间保存用户刚输入的参数，频率为 0 表示没有待提交值。 */
static PwmSettings pending_pwm_a;
static PwmSettings pending_pwm_b;
static bool sync_needs_match;
static bool calibrated;
static AppPage current_page = PAGE_MENU;
static TouchPoint calibration_targets[4];
static uint16_t calibration_samples[4][2];
static uint8_t calibration_index;
static uint32_t last_stopwatch_refresh_ms;
static uint32_t last_capture_refresh_ms;
static uint32_t last_snake_step_ms;
static bool sweep_start_blocked;

/*
 * 读取 KEY0 并做 20 ms 消抖。确认一次新的按下时返回 true，用于返回功能菜单。
 */
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

/* 返回菜单前结束页面编辑，保证再次打开原功能时会重绘整页。 */
static void App_ShowMenu(void)
{
    Ui_ResetPageState();
    current_page = PAGE_MENU;
    Menu_Show();
}

/* 将菜单选项交给已有页面入口，并为定期刷新的页面重新记录起点。 */
static void App_OpenMenuItem(MenuItem item)
{
    switch (item)
    {
        case MENU_PWM:
            current_page = PAGE_PWM;
            Ui_ShowPwm(dual_pwm.a);
            break;
        case MENU_STOPWATCH:
            current_page = PAGE_STOPWATCH;
            last_stopwatch_refresh_ms = Stopwatch_GetTickMs();
            Ui_ShowStopwatch(Stopwatch_GetElapsedMs(), Stopwatch_IsRunning(), Stopwatch_GetLaps());
            break;
        case MENU_CAPTURE:
            current_page = PAGE_CAPTURE;
            Ui_ShowCapture(Capture_GetResult());
            last_capture_refresh_ms = HAL_GetTick();
            break;
        case MENU_DUAL_PWM:
            current_page = PAGE_DUAL_PWM;
            Ui_ShowDualPwm(&dual_pwm);
            break;
        case MENU_SWEEP:
            current_page = PAGE_SWEEP;
            sweep_start_blocked = false;
            Ui_ShowSweep(Sweep_GetResult(), true, false);
            break;
        case MENU_SPWM:
            current_page = PAGE_SPWM;
            Ui_ShowSpwm(Spwm_IsRunning(), true);
            break;
        case MENU_SNAKE:
            current_page = PAGE_SNAKE;
            last_snake_step_ms = HAL_GetTick();
            Ui_ShowSnake(Snake_GetState(), true);
            break;
        case MENU_TILT_MAZE:
            current_page = PAGE_TILT_MAZE;
            TiltMaze_Open();
            break;
        case MENU_NONE:
            break;
    }
}

/* 启动外设和应用模块，触摸校准就绪后显示功能菜单。 */
void App_Init(void)
{
    /* 先提交符合上限的开机参数，再开启 PA11 输出。 */
    Pwm_Request(dual_pwm.a.frequency_hz, dual_pwm.a.duty_percent);
    Pwm_Init();

    /* TIM8 只需启动一次，之后硬件持续输出 B 路。 */
    PwmB_Request(dual_pwm.b.frequency_hz, dual_pwm.b.duty_percent);
    PwmB_Init();

    /* main.c 已初始化 TIM6，这里启动秒表的 1 ms 时间基准。 */
    Stopwatch_Init();

    /* TIM2 已由 main.c 初始化。生成正弦表，等触摸屏点 START 再调制。 */
    Spwm_Init();

    /* TIM5 在后台持续测量 PA0，功能三读取最新结果。 */
    Capture_Init();
    Snake_Reset(false, HAL_GetTick());

    TFTLCD_Init();
    calibrated = TouchInput_Init();

    if (calibrated)
    {
        App_ShowMenu();
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

/*
 * 处理秒表页面的触摸操作，并按刷新间隔更新计时显示。
 */
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

/* 提交给正确的定时器，驱动接受后更新两页共用的设定。 */
static void PwmApplySettings(bool channel_b, PwmSettings requested)
{
    if (channel_b)
    {
        if (PwmB_Request(requested.frequency_hz, requested.duty_percent))
        {
            dual_pwm.b = requested;
        }
    }
    else if (Pwm_Request(requested.frequency_hz, requested.duty_percent))
    {
        dual_pwm.a = requested;
    }
}

/* 修改频率时如果仍在同步，先退出，再交给主循环提交参数。 */
static void PwmSubmitSettings(bool channel_b, PwmSettings requested)
{
    const PwmSyncState sync = Pwm_GetSyncState();
    const PwmSettings current = channel_b ? dual_pwm.b : dual_pwm.a;
    PwmSettings *pending = channel_b ? &pending_pwm_b : &pending_pwm_a;

    if (sync != PWM_SYNC_OFF && requested.frequency_hz != current.frequency_hz)
    {
        Pwm_SetSync(false);
        sync_needs_match = true;
        *pending = requested;
    }
    else if (sync == PWM_SYNC_STOPPING)
    {
        *pending = requested;
    }
    else
    {
        PwmApplySettings(channel_b, requested);
    }
}

/* 保留另一项参数，只替换本次编辑的频率或占空比，再提交给对应通道。 */
static void PwmEditSettings(bool channel_b, bool edit_frequency, uint32_t value)
{
    const PwmSettings current = channel_b ? dual_pwm.b : dual_pwm.a;
    const PwmSettings pending = channel_b ? pending_pwm_b : pending_pwm_a;
    PwmSettings requested = pending.frequency_hz != 0U ? pending : current;
    if (edit_frequency)
    {
        requested.frequency_hz = value;
    }
    else
    {
        requested.duty_percent = (uint8_t)value;
    }
    PwmSubmitSettings(channel_b, requested);
}

/* 切换量程后将该通道频率限制在新上限内，并把占空比对齐到最近档。 */
static void PwmAdjustToRange(bool channel_b)
{
    const PwmSettings current = channel_b ? dual_pwm.b : dual_pwm.a;
    const PwmSettings pending = channel_b ? pending_pwm_b : pending_pwm_a;
    PwmSettings requested = pending.frequency_hz != 0U ? pending : current;
    const uint32_t maximum_hz = Pwm_GetMaxFrequencyHz();
    if (requested.frequency_hz > maximum_hz)
    {
        requested.frequency_hz = maximum_hz;
    }

    /* 占空比吸附到最近档，最后一档始终允许取配置的上限。 */
    const uint32_t step = Pwm_GetDutyStepPercent();
    const uint32_t duty = requested.duty_percent;
    const uint32_t lower = duty / step * step;
    const uint32_t upper = lower + step <= PWM_DUTY_MAX_PERCENT ? lower + step : PWM_DUTY_MAX_PERCENT;
    requested.duty_percent = (uint8_t)(duty - lower < upper - duty ? lower : upper);

    if (pending.frequency_hz != 0U ||
        requested.frequency_hz != current.frequency_hz || requested.duty_percent != current.duty_percent)
    {
        PwmSubmitSettings(channel_b, requested);
    }
}

/* 把驱动的同步状态换成页面提示。两路同频后清除频率提示。 */
static void DualPwm_UpdateSync(PwmSyncState sync)
{
    if (dual_pwm.a.frequency_hz == dual_pwm.b.frequency_hz &&
        pending_pwm_a.frequency_hz == 0U && pending_pwm_b.frequency_hz == 0U)
    {
        sync_needs_match = false;
    }

    switch (sync)
    {
        case PWM_SYNC_STARTING: dual_pwm.sync = DUAL_PWM_SYNC_STARTING; break;
        case PWM_SYNC_ON:       dual_pwm.sync = DUAL_PWM_SYNC_ON; break;
        case PWM_SYNC_STOPPING: dual_pwm.sync = DUAL_PWM_SYNC_STOPPING; break;
        case PWM_SYNC_OFF:
            dual_pwm.sync = sync_needs_match ? DUAL_PWM_SYNC_NEEDS_MATCH : DUAL_PWM_SYNC_OFF;
            break;
    }
}

/* 同步切换和待提交参数独立于触摸运行。只刷新正在显示的页面。 */
static void PwmControls_Task(void)
{
    Pwm_Task();
    const PwmSyncState sync = Pwm_GetSyncState();
    const bool apply_a = sync == PWM_SYNC_OFF && pending_pwm_a.frequency_hz != 0U;
    const bool apply_b = sync == PWM_SYNC_OFF && pending_pwm_b.frequency_hz != 0U;
    if (apply_a)
    {
        PwmApplySettings(false, pending_pwm_a);
        pending_pwm_a.frequency_hz = 0U;
    }
    if (apply_b)
    {
        PwmApplySettings(true, pending_pwm_b);
        pending_pwm_b.frequency_hz = 0U;
    }

    const DualPwmSync previous = dual_pwm.sync;
    DualPwm_UpdateSync(sync);
    if (current_page == PAGE_DUAL_PWM && (apply_a || apply_b || dual_pwm.sync != previous))
    {
        Ui_RefreshDualPwm(&dual_pwm);
    }
    if (current_page == PAGE_PWM && apply_a)
    {
        Ui_RefreshPwm(dual_pwm.a);
    }
}

/* 功能四的触摸事件提交给驱动，显示驱动接受的设定值。 */
static void DualPwm_HandleTouch(TouchEvent event)
{
    uint32_t value;
    switch (Ui_HandleDualPwmTouch(event, &dual_pwm, &value))
    {
        case UI_NO_CHANGE:
        case UI_TOGGLE_PWM_RANGE:
            return;
        case UI_CANCEL_EDIT:
            break;
        case UI_SET_FREQUENCY:
            PwmEditSettings(false, true, value);
            break;
        case UI_SET_DUTY:
            PwmEditSettings(false, false, value);
            break;
        case UI_SET_B_FREQUENCY:
            PwmEditSettings(true, true, value);
            break;
        case UI_SET_B_DUTY:
            PwmEditSettings(true, false, value);
            break;
        case UI_TOGGLE_SYNC:
        {
            const PwmSyncState sync = Pwm_GetSyncState();
            if (sync == PWM_SYNC_OFF)
            {
                sync_needs_match = !Pwm_SetSync(true);
            }
            else if (sync != PWM_SYNC_STOPPING)
            {
                Pwm_SetSync(false);
            }
            break;
        }
        case UI_PHASE_DOWN:
            if (dual_pwm.phase_deg > 0U)
            {
                /* 360° 若不在步进的整数档，减一次仍返回上一档。 */
                const uint16_t angle = ((dual_pwm.phase_deg - 1U) / PWM_PHASE_STEP_DEG) * PWM_PHASE_STEP_DEG;
                if (Pwm_SetPhase(angle))
                {
                    dual_pwm.phase_deg = angle;
                }
            }
            break;
        case UI_PHASE_UP:
        {
            const uint16_t next = dual_pwm.phase_deg + PWM_PHASE_STEP_DEG;
            const uint16_t angle = next <= 360U ? next : 360U;
            if (Pwm_SetPhase(angle))
            {
                dual_pwm.phase_deg = angle;
            }
            break;
        }
    }

    DualPwm_UpdateSync(Pwm_GetSyncState());
    Ui_ShowDualPwm(&dual_pwm);
}

/* 处理方向、重开和自动模式按钮，按当前速度推进游戏并刷新变化的格子。 */
static void SnakePage_Task(void)
{
    const uint32_t now = HAL_GetTick();
    const SnakeState *game = Snake_GetState();
    TouchEvent event;
    if (TouchInput_Poll(false, &event))
    {
        const UiSnakeAction action = Ui_HandleSnakeTouch(event);
        if (action == UI_SNAKE_TOGGLE_AUTO || action == UI_SNAKE_RESET)
        {
            Snake_Reset(action == UI_SNAKE_TOGGLE_AUTO ? !game->automatic : game->automatic, now);
            last_snake_step_ms = now;
            Ui_ShowSnake(game, false);
        }
        else if (action >= UI_SNAKE_UP && action <= UI_SNAKE_LEFT)
        {
            const bool ready = game->status == SNAKE_READY;
            Snake_SetDirection((SnakeDirection)action);
            if (ready && game->status == SNAKE_RUNNING)
            {
                last_snake_step_ms = now;
                Ui_ShowSnake(game, false);
            }
        }
    }

    /* 到时间只走一步，主循环继续处理实体键、PWM 和输入捕获。 */
    const uint32_t interval_ms = game->automatic ? 15U : 200U;
    if (game->status == SNAKE_RUNNING && now - last_snake_step_ms >= interval_ms)
    {
        last_snake_step_ms = now;
        Snake_Step();
        Ui_ShowSnake(game, false);
    }
}

/* 处理扫频启动按钮和拒绝提示，并把后台状态机的最新结果交给扫频页显示。 */
static void SweepPage_Task(void)
{
    TouchEvent event;
    if (TouchInput_Poll(false, &event) && Ui_SweepStartPressed(event) && !Sweep_IsRunning())
    {
        const bool started = Sweep_Start(dual_pwm.a.frequency_hz, dual_pwm.a.duty_percent);
        sweep_start_blocked = !started;
        Ui_ShowSweep(Sweep_GetResult(), started, sweep_start_blocked);
        return;
    }
    Ui_ShowSweep(Sweep_GetResult(), false, sweep_start_blocked);
}

/* HAL 的共用回调按定时器分发，秒表与 SPWM 各自处理自己的周期。 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        Stopwatch_OnPeriodElapsed();
    }
    else if (htim->Instance == TIM2)
    {
        Spwm_OnPeriodElapsed();
    }
}

/* SPWM 页面只在按下 START 或 STOP 时提交操作并刷新状态。 */
static void SpwmPage_Task(void)
{
    TouchEvent event;
    if (!TouchInput_Poll(false, &event) || !Ui_SpwmTogglePressed(event))
    {
        return;
    }

    if (Spwm_IsRunning())
    {
        Spwm_Stop();
    }
    else
    {
        Spwm_Start();
    }
    Ui_ShowSpwm(Spwm_IsRunning(), false);
}

/*
 * 主循环入口。推进 PWM 同步、输入捕获和扫频，再处理切页和触摸。
 */
void App_Task(void)
{
    PwmControls_Task();
    Capture_Task();
    Sweep_Task(HAL_GetTick());

    /* KEY0 在任一功能页返回菜单。菜单中也继续采样实体按键状态。 */
    if (calibrated && Key0_Pressed() && current_page != PAGE_MENU)
    {
        App_ShowMenu();
        return;
    }

    if (current_page == PAGE_SWEEP)
    {
        SweepPage_Task();
        return;
    }

    if (current_page == PAGE_SNAKE)
    {
        SnakePage_Task();
        return;
    }

    if (current_page == PAGE_TILT_MAZE)
    {
        TiltMaze_Task();
        return;
    }

    if (current_page == PAGE_SPWM)
    {
        SpwmPage_Task();
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
                    App_ShowMenu();
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

    if (current_page == PAGE_MENU)
    {
        App_OpenMenuItem(Menu_HandleTouch(event));
        return;
    }

    /* 扫频临时控制 A 路，结束前让 PWM 页面保持原有设定。 */
    if (Sweep_IsRunning())
    {
        return;
    }

    if (current_page == PAGE_DUAL_PWM)
    {
        DualPwm_HandleTouch(event);
        return;
    }

    /* 键盘确认或滑块跨档时，界面通过 &value 写入新值。 */
    uint32_t value;
    const UiAction action = Ui_HandleTouch(event, dual_pwm.a, &value);
    if (action == UI_NO_CHANGE)
    {
        return;
    }

    if (action == UI_TOGGLE_PWM_RANGE)
    {
        Pwm_SetExtendedMode(!Pwm_IsExtendedMode());
        PwmAdjustToRange(false);
        PwmAdjustToRange(true);
    }
    else
    {
        PwmEditSettings(false, action == UI_SET_FREQUENCY, value);
    }
    Ui_ShowPwm(dual_pwm.a);
}
