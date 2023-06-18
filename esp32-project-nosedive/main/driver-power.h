#ifndef DRIVER_POWER_H
#define DRIVER_POWER_H
void power_task(void *arg);
int power_info(struct task_descriptor *task, char *dest);
#endif // DRIVER_POWER_H
