#ifndef SECOND_TASK_CAPTURE_H
#define SECOND_TASK_CAPTURE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool valid;
    uint32_t period_ticks;
    uint32_t high_ticks;
    float frequency_hz;
    float duty_percent;
} CaptureResult;

void Capture_Init(void);
void Capture_Task(void);
const CaptureResult *Capture_GetResult(void);

#endif
