#ifndef SECOND_TASK_SPWM_H
#define SECOND_TASK_SPWM_H

#include <stdbool.h>

/* 载波频率要与 CubeMX 中 TIM2 的 PSC 和 ARR 对应。 */
#define SPWM_CARRIER_HZ 20000U
#define SPWM_SINE_HZ 50U

void Spwm_Init(void);
void Spwm_Start(void);
void Spwm_Stop(void);
bool Spwm_IsRunning(void);
void Spwm_OnPeriodElapsed(void);

#endif
