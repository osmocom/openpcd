#ifndef _PWM_H
#define _PWM_H

extern void pwm_freq_set(int channel, uint32_t freq);
extern void pwm_start(int channel);
extern void pwm_stop(int channel);
extern void pwm_duty_set_percent(int channel, uint16_t duty);
extern void pwm_init(void);
extern void pwm_fini(void);

#endif
