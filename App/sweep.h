#ifndef SECOND_TASK_SWEEP_H
#define SECOND_TASK_SWEEP_H

#include <stdbool.h>
#include <stdint.h>

#define SWEEP_POINT_COUNT 10U

typedef enum
{
    SWEEP_IDLE,
    SWEEP_RUNNING,
    SWEEP_PASS,
    SWEEP_FAIL
} SweepStatus;

typedef enum
{
    SWEEP_POINT_PENDING,
    SWEEP_POINT_PASS,
    SWEEP_POINT_OUT_OF_TOLERANCE,
    SWEEP_POINT_NO_SIGNAL,
    SWEEP_POINT_PWM_REJECTED
} SweepPointStatus;

typedef struct
{
    uint32_t set_frequency_hz;
    float measured_frequency_hz;
    float error_percent;
    SweepPointStatus status;
} SweepPoint;

typedef struct
{
    SweepStatus status;
    uint8_t completed;
    bool pwm_restored;
    SweepPoint points[SWEEP_POINT_COUNT];
} SweepResult;

bool Sweep_Start(uint32_t previous_frequency_hz, uint8_t previous_duty_percent);
void Sweep_Task(uint32_t now_ms);
bool Sweep_IsRunning(void);
const SweepResult *Sweep_GetResult(void);

#endif
