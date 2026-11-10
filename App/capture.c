#include "capture.h"

#include "tim.h"

#define TIM5_CLOCK_HZ 84000000U
#define CAPTURE_POLL_MS 10U
#define CAPTURE_TIMEOUT_MS 1500U

static bool capture_started;
static bool seen_rising;
static uint32_t last_poll_ms;
static CaptureResult capture_result;

/* 在 PA0 启动 TIM5 的 PWM 输入捕获。上升沿捕获周期并清零，下降沿捕获高电平宽度。 */
void Capture_Init(void)
{
    /* TI1FP1 的上升沿先捕获周期，然后让 TIM5 从零开始数下一周期。 */
    const TIM_SlaveConfigTypeDef reset_on_rising = {
        .SlaveMode = TIM_SLAVEMODE_RESET,
        .InputTrigger = TIM_TS_TI1FP1,
        .TriggerPolarity = TIM_TRIGGERPOLARITY_RISING,
        .TriggerPrescaler = TIM_TRIGGERPRESCALER_DIV1,
        .TriggerFilter = 0U,
    };
    if (HAL_TIM_SlaveConfigSynchro(&htim5, &reset_on_rising) != HAL_OK)
    {
        return;
    }

    __HAL_TIM_CLEAR_FLAG(&htim5, TIM_FLAG_CC1 | TIM_FLAG_CC2 | TIM_FLAG_CC1OF);
    if (HAL_TIM_IC_Start(&htim5, TIM_CHANNEL_2) != HAL_OK)
    {
        return;
    }
    if (HAL_TIM_IC_Start(&htim5, TIM_CHANNEL_1) != HAL_OK)
    {
        HAL_TIM_IC_Stop(&htim5, TIM_CHANNEL_2);
        return;
    }
    capture_started = true;
}

/* 按毫秒刻度轮询 TIM5，校验周期和脉宽，发布频率、占空比以及样本时刻。 */
void Capture_Task(void)
{
    const uint32_t now = HAL_GetTick();
    if (!capture_started || now - last_poll_ms < CAPTURE_POLL_MS)
    {
        return;
    }
    last_poll_ms = now;

    if (now - capture_result.sample_ms >= CAPTURE_TIMEOUT_MS)
    {
        capture_result.valid = false;
    }

    const uint32_t flags = htim5.Instance->SR;
    if ((flags & TIM_FLAG_CC1) == 0U)
    {
        return;
    }

    if (!seen_rising)
    {
        seen_rising = true;
        if ((flags & TIM_FLAG_CC1OF) == 0U)
        {
            /* 第一个上升沿只用来清零。CC1OF 表示已经捕获过多个上升沿。 */
            (void)HAL_TIM_ReadCapturedValue(&htim5, TIM_CHANNEL_1);
            return;
        }
    }

    if ((flags & TIM_FLAG_CC2) == 0U)
    {
        return;
    }

    /* 读 CCR 会清除对应捕获标志，等下一组边沿再更新结果。 */
    const uint32_t period = HAL_TIM_ReadCapturedValue(&htim5, TIM_CHANNEL_1);
    const uint32_t high = HAL_TIM_ReadCapturedValue(&htim5, TIM_CHANNEL_2);
    if (high == 0U || high >= period || period > TIM5_CLOCK_HZ + TIM5_CLOCK_HZ / 20U)
    {
        return;
    }

    capture_result.period_ticks = period;
    capture_result.high_ticks = high;
    capture_result.frequency_hz = (float)TIM5_CLOCK_HZ / (float)period;
    capture_result.duty_percent = 100.0f * (float)high / (float)period;
    capture_result.valid = true;
    capture_result.sample_ms = now;
}

/* 返回最近的测量结果。调用方先检查 valid，再使用测量值。 */
const CaptureResult *Capture_GetResult(void)
{
    return &capture_result;
}
