#ifndef SECOND_TASK_APP_CONFIG_H
#define SECOND_TASK_APP_CONFIG_H

/* 运行时在普通量程和扩展量程间切换。 */
#define PWM_STANDARD_MAX_HZ 200000U
#define PWM_EXTENDED_MAX_HZ 500000U
#define PWM_EXTENDED_DUTY_STEP_PERCENT 5U

/* PWM 页面允许设定的最大占空比，取值 1～100，单位为百分比。 */
#define PWM_DUTY_MAX_PERCENT 100U

#if PWM_DUTY_MAX_PERCENT < 1U || PWM_DUTY_MAX_PERCENT > 100U
#error "PWM_DUTY_MAX_PERCENT must be in the range 1 to 100"
#endif

/* 普通模式下占空比每档相差的百分点。 */
#define PWM_DUTY_STEP_PERCENT 1U

#if PWM_DUTY_STEP_PERCENT < 1U || PWM_DUTY_STEP_PERCENT > 100U
#error "PWM_DUTY_STEP_PERCENT must be in the range 1 to 100"
#endif

/* 双路 PWM 相位每档的角度，单位为度。 */
#define PWM_PHASE_STEP_DEG 5U

#if PWM_PHASE_STEP_DEG < 1U || PWM_PHASE_STEP_DEG > 360U
#error "PWM_PHASE_STEP_DEG must be in the range 1 to 360"
#endif

#endif
