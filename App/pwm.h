#ifndef SECOND_TASK_PWM_H
#define SECOND_TASK_PWM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t frequency_hz;
    uint8_t duty_percent;
    bool pending;
} PwmStatus;

void Pwm_Init(void);
bool Pwm_Request(uint32_t frequency_hz, uint32_t duty_percent);
void Pwm_Task(void);
PwmStatus Pwm_GetStatus(void);

#endif /* SECOND_TASK_PWM_H */
