#include "pwm.h"

#include "app_config.h"
#include "tim.h"

#define PWM_TIMER_CLOCK_HZ 168000000UL  /* CubeMX 配置的 TIM1 时钟，单位 Hz */
#define PWM_MAX_HZ 200000U

void Pwm_Init(void)
{
    /* 先加载开机参数，PA11 从第一个周期就使用设定值。 */
    htim1.Instance->EGR = TIM_EGR_UG;
    (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
}

bool Pwm_Request(uint32_t frequency_hz, uint32_t duty_percent)
{
    if (frequency_hz == 0U || frequency_hz > PWM_MAX_HZ || duty_percent > PWM_DUTY_MAX_PERCENT)
    {
        return false;
    }

    /*
     * PWM 频率 = 定时器时钟 / 分频倍数 / 每周期计数。
     * 选最小可用的分频倍数，让每周期最多数 65535 次，
     * 这样占空比设为 100% 时，CCR4 也装得下整周期的计数。
     */
    const uint64_t max_denominator = (uint64_t)frequency_hz * UINT16_MAX;
    const uint32_t divider =
        (uint32_t)((PWM_TIMER_CLOCK_HZ + max_denominator - 1U) / max_denominator);
    /* 计算每周期的计数次数。整数除法前加半个除数，用于四舍五入。 */
    const uint64_t denominator = (uint64_t)frequency_hz * divider;
    const uint32_t counts =
        (uint32_t)((PWM_TIMER_CLOCK_HZ + denominator / 2U) / denominator);
    /* PWM 模式 1 下，比较值就是一个周期内的高电平计数。 */
    const uint32_t compare = (counts * duty_percent + 50U) / 100U;

    /* 短暂关中断，并暂停 TIM1 的更新事件，避免只应用了一部分新参数。 */
    TIM_TypeDef *timer = htim1.Instance;
    const uint32_t irq_mask = __get_PRIMASK();
    __disable_irq();
    SET_BIT(timer->CR1, TIM_CR1_UDIS);
    timer->PSC = divider - 1U;  /* 实际分频为 PSC + 1 */
    timer->ARR = counts - 1U;   /* CNT 从 0 数到 ARR */
    timer->CCR4 = compare;      /* CNT 小于 CCR4 时输出高电平 */
    /* 恢复更新后，三个参数在下一次自然更新边界一起生效。 */
    CLEAR_BIT(timer->CR1, TIM_CR1_UDIS);
    __set_PRIMASK(irq_mask);

    return true;
}
