#include "stopwatch.h"

#include <string.h>

#include "tim.h"

/* 中断更新毫秒计数，主循环通过 Stopwatch_GetTickMs() 读取。 */
static volatile uint32_t tick_ms;

/* 下面的秒表状态只由主循环读写。 */
static bool running;
static uint32_t start_tick;
static uint32_t accumulated_ms;
static StopwatchLap laps[STOPWATCH_LAP_CAPACITY];  /* 最新记录排在首位。 */

void Stopwatch_Init(void)
{
    /* 清掉初始化留下的更新标志，等第一个完整周期再计入 1 ms。 */
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    HAL_TIM_Base_Start_IT(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* HAL 共用这个回调，只累加来自 TIM6 的更新事件。 */
    if (htim->Instance == TIM6)
    {
        ++tick_ms;
    }
}

uint32_t Stopwatch_GetTickMs(void)
{
    return tick_ms;
}

void Stopwatch_ToggleRunning(void)
{
    const uint32_t now = Stopwatch_GetTickMs();
    if (running)
    {
        /* 暂停时只把本次运行的时间加进累计值。 */
        accumulated_ms += now - start_tick;
    }
    else
    {
        /* 继续时从当前刻度重新开始，跳过暂停期间。 */
        start_tick = now;
    }
    running = !running;
}

void Stopwatch_Reset(void)
{
    running = false;
    accumulated_ms = 0;
    memset(laps, 0, sizeof laps);
}

bool Stopwatch_IsRunning(void)
{
    return running;
}

uint32_t Stopwatch_GetElapsedMs(void)
{
    return running ? accumulated_ms + (Stopwatch_GetTickMs() - start_tick) : accumulated_ms;
}

void Stopwatch_RecordLap(void)
{
    if (!running)
    {
        return;
    }

    /* 一次读取同时用于累计时间和分段时间，保证两列对应同一时刻。 */
    const uint32_t total_ms = Stopwatch_GetElapsedMs();
    const StopwatchLap lap = {
        .number = laps[0].number + 1U,
        .total_ms = total_ms,
        .split_ms = total_ms - laps[0].total_ms,
    };

    /* 旧记录后移一行，屏幕保留最近四圈。 */
    memmove(&laps[1], &laps[0], sizeof laps - sizeof laps[0]);
    laps[0] = lap;
}

const StopwatchLap *Stopwatch_GetLaps(void)
{
    return laps;
}
