#ifndef SECOND_TASK_PWM_H
#define SECOND_TASK_PWM_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    PWM_SYNC_OFF,
    PWM_SYNC_STARTING,
    PWM_SYNC_ON,
    PWM_SYNC_STOPPING
} PwmSyncState;

bool Pwm_IsExtendedMode(void);
void Pwm_SetExtendedMode(bool enabled);
uint32_t Pwm_GetMaxFrequencyHz(void);
uint32_t Pwm_GetDutyStepPercent(void);
void Pwm_Init(void);
bool Pwm_Request(uint32_t frequency_hz, uint32_t duty_percent);
void PwmB_Init(void);
bool PwmB_Request(uint32_t frequency_hz, uint32_t duty_percent);
bool Pwm_SetPhase(uint32_t angle_deg);
bool Pwm_SetSync(bool enabled);
PwmSyncState Pwm_GetSyncState(void);
void Pwm_Task(void);

#endif /* SECOND_TASK_PWM_H */
