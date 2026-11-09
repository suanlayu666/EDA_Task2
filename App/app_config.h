#ifndef SECOND_TASK_APP_CONFIG_H
#define SECOND_TASK_APP_CONFIG_H

/* PWM 页面允许设定的最大占空比，取值 1～100，单位为百分比。 */
#define PWM_DUTY_MAX_PERCENT 100U

#if PWM_DUTY_MAX_PERCENT < 1U || PWM_DUTY_MAX_PERCENT > 100U
#error "PWM_DUTY_MAX_PERCENT must be in the range 1 to 100"
#endif

/* 占空比每档相差的百分点。改这里可同时调整数字键盘和滑块。 */
#define PWM_DUTY_STEP_PERCENT 1U

#if PWM_DUTY_STEP_PERCENT < 1U || PWM_DUTY_STEP_PERCENT > 100U
#error "PWM_DUTY_STEP_PERCENT must be in the range 1 to 100"
#endif

#endif
