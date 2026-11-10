#include "spwm.h"

#include <math.h>
#include <stdint.h>

#include "tim.h"

#define SPWM_SAMPLE_COUNT (SPWM_CARRIER_HZ / SPWM_SINE_HZ)

static uint16_t spwm_table[SPWM_SAMPLE_COUNT];
static uint16_t next_sample;
static bool carrier_started;
static bool running;

/* 开机生成一次正弦表。启动 TIM2_CH4，先让 PA3 输出 0% 占空比。 */
void Spwm_Init(void)
{
    const uint32_t period_counts = htim2.Instance->ARR + 1U;
    for (uint32_t i = 0; i < SPWM_SAMPLE_COUNT; ++i)
    {
        const float angle = 6.2831853f * (float)i / (float)SPWM_SAMPLE_COUNT;
        const float duty = 0.5f + 0.4f * sinf(angle);
        spwm_table[i] = (uint16_t)((float)period_counts * duty + 0.5f);
    }

    htim2.Instance->CCR4 = 0U;
    htim2.Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    carrier_started = HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4) == HAL_OK;
}

/* START 从正弦表的开头重新输出，并打开 TIM2 的周期更新中断。 */
void Spwm_Start(void)
{
    if (running || !carrier_started)
    {
        return;
    }

    /* 先加载第 0 项，再把第 1 项放进预装载寄存器，供下一个周期使用。 */
    htim2.Instance->CCR4 = spwm_table[0];
    htim2.Instance->EGR = TIM_EGR_UG;
    htim2.Instance->CCR4 = spwm_table[1];
    next_sample = 2U;

    /* 清掉启动产生的更新标志，等下一个完整周期再进中断。 */
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    running = true;
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
}

/* STOP 先关中断，再将占空比清零。下一个载波周期起，PA3 保持低电平。 */
void Spwm_Stop(void)
{
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_UPDATE);
    htim2.Instance->CCR4 = 0U;
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    running = false;
}

/* 提供实际的开关状态，让屏幕决定显示 START 还是 STOP。 */
bool Spwm_IsRunning(void)
{
    return running;
}

/* TIM2 每 50 µs 调用一次。只查表、写 CCR4，并把下标移到下一项。 */
void Spwm_OnPeriodElapsed(void)
{
    htim2.Instance->CCR4 = spwm_table[next_sample];
    ++next_sample;
    if (next_sample == SPWM_SAMPLE_COUNT)
    {
        next_sample = 0U;
    }
}
