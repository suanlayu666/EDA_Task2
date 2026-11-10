#include "sweep.h"

#include "app_config.h"
#include "capture.h"
#include "pwm.h"

#define SWEEP_DUTY_PERCENT (PWM_DUTY_MAX_PERCENT < 50U ? PWM_DUTY_MAX_PERCENT : 50U)
#define SWEEP_TOLERANCE_PERCENT 1.0f
#define CAPTURE_MARGIN_MS 20U
#define SWEEP_SAMPLE_TIMEOUT_MS 500U

/* 固定列是设定频率，其余列由每次测量填写。 */
static SweepResult sweep_result = {
    .points = {
        {.set_frequency_hz = 10U}, {.set_frequency_hz = 20U},
        {.set_frequency_hz = 50U}, {.set_frequency_hz = 100U},
        {.set_frequency_hz = 500U}, {.set_frequency_hz = 1000U},
        {.set_frequency_hz = 5000U}, {.set_frequency_hz = 20000U},
        {.set_frequency_hz = 100000U}, {.set_frequency_hz = 200000U},
    },
};

static bool point_started;
static uint32_t restore_frequency_hz;
static uint8_t restore_duty_percent;
static uint32_t last_frequency_hz;
static uint32_t step_started_ms;
static uint32_t settle_ms;
static bool all_points_passed;

/* 把以 Hz 表示的频率换算为向上取整的周期毫秒数。 */
static uint32_t PeriodMs(uint32_t frequency_hz)
{
    /* 不足 1 ms 的周期按 1 ms 计算，等待时间向上取整。 */
    return (1000U + frequency_hz - 1U) / frequency_hz;
}

/* 在 PWM 独立模式下启动十点扫频，清理旧结果并保存结束后要恢复的 A 路参数。 */
bool Sweep_Start(uint32_t previous_frequency_hz, uint8_t previous_duty_percent)
{
    if (Sweep_IsRunning() || Pwm_GetSyncState() != PWM_SYNC_OFF || previous_frequency_hz == 0U)
    {
        return false;
    }

    sweep_result.status = SWEEP_RUNNING;
    sweep_result.completed = 0U;
    sweep_result.pwm_restored = false;
    for (unsigned int i = 0; i < SWEEP_POINT_COUNT; ++i)
    {
        SweepPoint *point = &sweep_result.points[i];
        *point = (SweepPoint){.set_frequency_hz = point->set_frequency_hz};
    }
    restore_frequency_hz = previous_frequency_hz;
    restore_duty_percent = previous_duty_percent;
    /* 手动频率可能还在预装载，第一行按最长的旧周期 1 秒等待。 */
    last_frequency_hz = 1U;
    all_points_passed = true;
    point_started = false;
    return true;
}

/* 记录当前频点的结果并推进到下一点，最后一项结束后恢复 A 路原设定。 */
static void FinishPoint(SweepPoint *point, SweepPointStatus status)
{
    point->status = status;
    if (status != SWEEP_POINT_PASS)
    {
        all_points_passed = false;
    }

    point_started = false;
    ++sweep_result.completed;
    if (sweep_result.completed == SWEEP_POINT_COUNT)
    {
        sweep_result.pwm_restored = Pwm_Request(restore_frequency_hz, restore_duty_percent);
        sweep_result.status = all_points_passed && sweep_result.pwm_restored ? SWEEP_PASS : SWEEP_FAIL;
    }
}

/* 推进扫频的设频、等待、读取新样本和判定流程，每次调用只处理当前阶段。 */
void Sweep_Task(uint32_t now_ms)
{
    if (!Sweep_IsRunning())
    {
        return;
    }

    SweepPoint *point = &sweep_result.points[sweep_result.completed];
    const uint32_t frequency_hz = point->set_frequency_hz;
    if (!point_started)
    {
        if (!Pwm_Request(frequency_hz, SWEEP_DUTY_PERCENT))
        {
            FinishPoint(point, SWEEP_POINT_PWM_REJECTED);
            return;
        }
        /* 等旧周期结束，再给输入捕获留两个新周期。 */
        settle_ms = PeriodMs(last_frequency_hz) + 2U * PeriodMs(frequency_hz) + CAPTURE_MARGIN_MS;
        last_frequency_hz = frequency_hz;
        step_started_ms = now_ms;
        point_started = true;
        return;
    }

    const uint32_t elapsed = now_ms - step_started_ms;
    if (elapsed < settle_ms)
    {
        return;
    }

    const CaptureResult *capture = Capture_GetResult();
    const uint32_t sample_elapsed = capture->sample_ms - step_started_ms;
    /* 样本必须在本频点稳定之后发布。较早的样本不属于这一行。 */
    if (capture->valid && sample_elapsed >= settle_ms && sample_elapsed <= elapsed)
    {
        point->measured_frequency_hz = capture->frequency_hz;
        const float set_hz = (float)frequency_hz;
        const float difference = point->measured_frequency_hz - set_hz;
        const float abs_difference = difference < 0.0f ? -difference : difference;
        point->error_percent = 100.0f * abs_difference / set_hz;
        FinishPoint(point, abs_difference <= set_hz * SWEEP_TOLERANCE_PERCENT / 100.0f
                          ? SWEEP_POINT_PASS : SWEEP_POINT_OUT_OF_TOLERANCE);
    }
    else if (elapsed - settle_ms >= SWEEP_SAMPLE_TIMEOUT_MS)
    {
        FinishPoint(point, SWEEP_POINT_NO_SIGNAL);
    }
}

/* 返回扫频是否正在使用 A 路 PWM 输出。 */
bool Sweep_IsRunning(void)
{
    return sweep_result.status == SWEEP_RUNNING;
}

/* 返回只读扫频结果，包括逐点数据和总判定。 */
const SweepResult *Sweep_GetResult(void)
{
    return &sweep_result;
}
