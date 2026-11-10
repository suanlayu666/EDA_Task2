#ifndef SECOND_TASK_STOPWATCH_H
#define SECOND_TASK_STOPWATCH_H

#include <stdbool.h>
#include <stdint.h>

enum { STOPWATCH_LAP_CAPACITY = 4 };

typedef struct
{
    uint32_t number;
    uint32_t total_ms;
    uint32_t split_ms;
} StopwatchLap;

void Stopwatch_Init(void);
void Stopwatch_OnPeriodElapsed(void);
uint32_t Stopwatch_GetTickMs(void);
void Stopwatch_ToggleRunning(void);
void Stopwatch_Reset(void);
bool Stopwatch_IsRunning(void);
uint32_t Stopwatch_GetElapsedMs(void);
void Stopwatch_RecordLap(void);
const StopwatchLap *Stopwatch_GetLaps(void);

#endif
