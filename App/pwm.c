#include "pwm.h"

#include "app_config.h"
#include "tim.h"

#define PWM_TIMER_CLOCK_HZ 168000000UL  /* TIM1 和 TIM8 共用的 APB2 定时器时钟，单位 Hz。 */
#define PWM_MAX_HZ 200000U
#define SYNC_B_DIVIDER_SCALE 3U

typedef struct
{
    uint32_t divider;
    uint32_t counts;
} PwmTiming;

static PwmTiming timing_a;
static uint32_t frequency_a;
static uint32_t frequency_b;
static uint32_t duty_b;
static bool sync_requested;
static bool sync_active;
static uint32_t phase_deg;
static uint32_t phase_applied;
static uint32_t phase_staged;
static bool phase_pending;
static uint32_t sync_b_compare;
static bool sync_duty_pending;
static bool sync_duty_requested;

/*
 * 检查请求的频率和占空比能否使用。
 * frequency_hz 的单位是 Hz，范围为 1～200000。
 * duty_percent 的单位是百分比，范围为 0～配置的占空比上限。
 * 两项都符合范围才返回 true。
 */
static bool Pwm_Valid(uint32_t frequency_hz, uint32_t duty_percent)
{
    return frequency_hz != 0U && frequency_hz <= PWM_MAX_HZ &&
           duty_percent <= PWM_DUTY_MAX_PERCENT;
}

/*
 * 把已经通过校验的频率换算成实际分频倍数 divider 和每周期计数次数 counts。
 * 例如 1000 Hz 得到 divider = 3、counts = 56000，调用方再把它们换成 PSC 和 ARR。
 * 这里只计算参数，调用写入函数后才会修改定时器。
 */
static PwmTiming Pwm_CalculateTiming(uint32_t frequency_hz)
{
    /*
     * PWM 频率 = 定时器时钟 / 分频倍数 / 每周期计数。
     * 选最小可用的分频倍数，让每周期最多数 65535 次，
     * 这样占空比设为 100% 时，比较寄存器也装得下整周期的计数。
     */
    const uint64_t max_denominator = (uint64_t)frequency_hz * UINT16_MAX;
    const uint32_t divider =
        (uint32_t)((PWM_TIMER_CLOCK_HZ + max_denominator - 1U) / max_denominator);
    /* 计算每周期的计数次数。整数除法前加半个除数，用于四舍五入。 */
    const uint64_t denominator = (uint64_t)frequency_hz * divider;
    const uint32_t counts =
        (uint32_t)((PWM_TIMER_CLOCK_HZ + denominator / 2U) / denominator);
    return (PwmTiming){divider, counts};
}

/*
 * 把一组分频、周期和比较值写到指定的定时器通道。
 * htim 指定 TIM1 或 TIM8，channel 指定通道，compare 是写入 CCR 的计数值。
 * 写入期间暂时禁止更新事件，避免定时器只加载其中一部分参数。
 * 清除旧的更新标志，调用方可用下一个更新标志判断这组参数何时加载。
 */
static void Pwm_WriteTiming(TIM_HandleTypeDef *htim, uint32_t channel,
                            PwmTiming timing, uint32_t compare)
{
    /* 暂停更新事件，让分频、周期和占空比在下一次更新时一起生效。 */
    TIM_TypeDef *timer = htim->Instance;
    const uint32_t irq_mask = __get_PRIMASK();
    __disable_irq();
    SET_BIT(timer->CR1, TIM_CR1_UDIS);
    __HAL_TIM_CLEAR_FLAG(htim, TIM_FLAG_UPDATE);
    timer->PSC = timing.divider - 1U;  /* 实际分频为 PSC + 1。 */
    timer->ARR = timing.counts - 1U;   /* CNT 从 0 数到 ARR。 */
    __HAL_TIM_SET_COMPARE(htim, channel, compare);
    CLEAR_BIT(timer->CR1, TIM_CR1_UDIS);
    __set_PRIMASK(irq_mask);
}

/*
 * 给独立运行的一路配置周期和占空比。
 * 先把 duty_percent 换算成高电平计数，再交给 Pwm_WriteTiming() 一起写入。
 */
static void Pwm_WriteIndependent(TIM_HandleTypeDef *htim, uint32_t channel,
                                 PwmTiming timing, uint32_t duty_percent)
{
    const uint32_t compare = (timing.counts * duty_percent + 50U) / 100U;
    Pwm_WriteTiming(htim, channel, timing, compare);
}

/*
 * 用 A 路的周期计数，把目标角度 phase_deg 换算成 TIM1 CH1 的比较位置。
 * 返回值就是 TIM1 的 CNT 应该数到几才触发 TIM8。360° 和 0° 对应同一个位置。
 */
static uint32_t Pwm_PhaseCount(void)
{
    return (timing_a.counts * (phase_deg % 360U) + 180U) / 360U;
}

/*
 * 把 count 写进 TIM1 的 CCR1 预装载寄存器，等下一次更新才用于比较。
 * phase_staged 记录本次写入的计数，phase_pending 告诉 Pwm_Task() 还有一次更新要确认。
 */
static void Pwm_StagePhase(uint32_t count)
{
    /* 等 TIM1 的下一次更新同时加载周期边界和 CH1 比较值。 */
    SET_BIT(htim1.Instance->CR1, TIM_CR1_UDIS);
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);
    htim1.Instance->CCR1 = count;
    CLEAR_BIT(htim1.Instance->CR1, TIM_CR1_UDIS);
    phase_staged = count;
    phase_pending = true;
}

/*
 * 为同步运行的 B 路设置 TIM8 的分频、计数上限和高电平宽度。
 * 同步时每次 TIM1 触发都会让 TIM8 开始新一轮。较大的计数上限给调相过渡留余量。
 * CCR2 按 A 路的周期和 B 路的分频计算，让 B 路保留自己的占空比设定。
 */
static void Pwm_WriteSyncB(void)
{
    /* 同步时由触发信号决定周期。加大 ARR 和分频，为调相后的长周期留余量。 */
    const PwmTiming timing = {timing_a.divider * SYNC_B_DIVIDER_SCALE, UINT16_MAX + 1U};
    const uint32_t scale = 100U * SYNC_B_DIVIDER_SCALE;
    sync_b_compare = duty_b == 100U ? UINT16_MAX : (timing_a.counts * duty_b + scale / 2U) / scale;
    Pwm_WriteTiming(&htim8, TIM_CHANNEL_2, timing, sync_b_compare);
    sync_duty_pending = true;
}

/*
 * 配置 TIM1 CH1 的内部比较触发，加载初始参数，再启动 TIM1 CH4 的 A 路输出。
 * 由 App_Init() 在开机时调用一次。之后 PA11 的方波由硬件重复输出。
 */
void Pwm_Init(void)
{
    /* CH1 在内部做比较，命中 CCR1 时通过 TRGO 发出触发脉冲。 */
    MODIFY_REG(htim1.Instance->CCMR1,
               TIM_CCMR1_CC1S | TIM_CCMR1_OC1M | TIM_CCMR1_OC1FE, TIM_CCMR1_OC1PE);
    MODIFY_REG(htim1.Instance->CR2, TIM_CR2_MMS, TIM_TRGO_OC1);

    /* 先加载开机参数，再开启 A 路 PA11。 */
    htim1.Instance->EGR = TIM_EGR_UG;
    (void)HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
}

/*
 * 提交 A 路的频率（Hz）和占空比（百分比）。
 * 独立模式下计算整组参数。请求同步后，频率锁定，只更新 A 路的 CCR4。
 * 返回 true 表示请求已接受，预装载的值会在定时器更新时生效。
 */
bool Pwm_Request(uint32_t frequency_hz, uint32_t duty_percent)
{
    if (!Pwm_Valid(frequency_hz, duty_percent) ||
        (sync_active && !sync_requested) || (sync_requested && frequency_hz != frequency_a))
    {
        return false;
    }

    if (sync_requested)
    {
        htim1.Instance->CCR4 = (timing_a.counts * duty_percent + 50U) / 100U;
    }
    else
    {
        timing_a = Pwm_CalculateTiming(frequency_hz);
        frequency_a = frequency_hz;
        Pwm_WriteIndependent(&htim1, TIM_CHANNEL_4, timing_a, duty_percent);
    }
    return true;
}

/*
 * 加载 TIM8 的初始参数，再启动 TIM8 CH2 的 B 路输出。
 * 由 App_Init() 在开机时调用一次。之后 PC7 的方波由硬件重复输出。
 */
void PwmB_Init(void)
{
    /* 先加载开机参数，再开启 B 路 PC7。 */
    htim8.Instance->EGR = TIM_EGR_UG;
    (void)HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
}

/*
 * 提交 B 路的频率（Hz）和占空比（百分比）。
 * 独立模式下按 B 路频率计算周期。同步时周期跟随 A 路，B 路可以单独调整占空比。
 * 参数超限、同步中单独改频、切换期间暂不能写入时返回 false。
 */
bool PwmB_Request(uint32_t frequency_hz, uint32_t duty_percent)
{
    if (!Pwm_Valid(frequency_hz, duty_percent) ||
        (sync_active && !sync_requested) ||
        (sync_requested && frequency_hz != frequency_b))
    {
        return false;
    }

    frequency_b = frequency_hz;
    duty_b = duty_percent;
    if (sync_active)
    {
        /* 由主循环等相位写入结束，再提交新脉宽。 */
        sync_duty_requested = true;
    }
    else
    {
        Pwm_WriteIndependent(&htim8, TIM_CHANNEL_2, Pwm_CalculateTiming(frequency_hz), duty_percent);
    }
    return true;
}

/*
 * 保存 B 路相对 A 路的目标延迟角度，单位为度，允许 0～360。
 * 返回 true 表示已保存目标。同步开启时，Pwm_Task() 会把角度换成比较位置并更新硬件。
 */
bool Pwm_SetPhase(uint32_t angle_deg)
{
    if (angle_deg > 360U)
    {
        return false;
    }
    phase_deg = angle_deg;
    return true;
}

/*
 * 请求开启或退出同步。开启前，A、B 的频率设定必须相同。
 * 退出时先给 TIM8 提交独立参数，等下一次更新后再由 Pwm_Task() 断开内部触发。
 * 返回 true 表示接受切换请求，实际切换要等定时器边界。
 */
bool Pwm_SetSync(bool enabled)
{
    if (enabled && (frequency_a == 0U || frequency_a != frequency_b ||
                    (sync_active && !sync_requested)))
    {
        return false;
    }

    if (!enabled && sync_requested)
    {
        if (sync_active)
        {
            /* 先用最后一次同步触发加载独立参数，再断开触发。 */
            Pwm_WriteIndependent(&htim8, TIM_CHANNEL_2, Pwm_CalculateTiming(frequency_b), duty_b);
        }
        else
        {
            phase_pending = false;
        }
    }
    sync_requested = enabled;
    return true;
}

/* TIM8 确实收到内部触发之后，界面才显示同步已开启。 */
PwmSyncState Pwm_GetSyncState(void)
{
    if (!sync_requested)
    {
        return sync_active ? PWM_SYNC_STOPPING : PWM_SYNC_OFF;
    }

    return sync_active && __HAL_TIM_GET_FLAG(&htim8, TIM_FLAG_TRIGGER)
           ? PWM_SYNC_ON : PWM_SYNC_STARTING;
}

/*
 * 在主循环推进同步和调相。根据定时器更新标志确认参数已加载，再进行下一步。
 * 开启时等 A 路加载新周期，再接通 TIM1 TRGO 到 TIM8 ITR0。退出时等 B 路加载独立周期再断开。
 * 同步中如果 B 路要大幅提前，会分几个周期移动触发点，给每个脉冲留出低电平时间。
 */
void Pwm_Task(void)
{
    if (!sync_requested)
    {
        if (sync_active && __HAL_TIM_GET_FLAG(&htim8, TIM_FLAG_UPDATE))
        {
            CLEAR_BIT(htim8.Instance->SMCR, TIM_SMCR_SMS);
            sync_active = false;
            phase_pending = false;
            sync_duty_pending = false;
            sync_duty_requested = false;
        }
        return;
    }

    if (!sync_active)
    {
        if (!__HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_UPDATE))
        {
            return;  /* 等 A 路刚提交的频率加载到实际计数器。 */
        }

        if (!phase_pending)
        {
            Pwm_StagePhase(Pwm_PhaseCount());
            return;
        }

        /* A 路的相位已加载，TIM8 现在可以接通触发。 */
        phase_applied = phase_staged;
        phase_pending = false;
        Pwm_WriteSyncB();
        __HAL_TIM_CLEAR_FLAG(&htim8, TIM_FLAG_TRIGGER);
        /* STM32F407 的 TIM8 ITR0 连接 TIM1 TRGO。 */
        MODIFY_REG(htim8.Instance->SMCR, TIM_SMCR_TS | TIM_SMCR_SMS,
                   TIM_TS_ITR0 | TIM_SLAVEMODE_RESET);
        sync_active = true;
        return;
    }

    if (phase_pending)
    {
        if (!__HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_UPDATE))
        {
            return;
        }
        phase_applied = phase_staged;
        phase_pending = false;
    }

    if (sync_duty_pending)
    {
        if (!__HAL_TIM_GET_FLAG(&htim8, TIM_FLAG_UPDATE))
        {
            return;
        }
        sync_duty_pending = false;
    }

    if (sync_duty_requested)
    {
        Pwm_WriteSyncB();
        sync_duty_requested = false;
        return;
    }

    uint32_t next = Pwm_PhaseCount();
    if (next < phase_applied && sync_b_compare != 0U && sync_b_compare != UINT16_MAX)
    {
        /* 提前触发会缩短 B 路的一个周期，留至少一个 B 计数给低电平。 */
        const uint32_t minimum_period = (sync_b_compare + 1U) * SYNC_B_DIVIDER_SCALE;
        const uint32_t max_advance = timing_a.counts - minimum_period;
        if (phase_applied - next > max_advance)
        {
            next = phase_applied - max_advance;
        }
    }
    if (next != phase_applied)
    {
        Pwm_StagePhase(next);
    }
}
