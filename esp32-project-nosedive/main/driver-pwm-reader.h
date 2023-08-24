#ifndef DRIVER_PWM_READER_H
#define DRIVER_PWM_READER_H
void trigger_pwm_calculation(void *pwm_set_id);
void pwm_reader_task(void *arg);
int pwm_reader_info(struct task_descriptor *task, char *dest);
#endif // DRIVER_PWM_READER_H
