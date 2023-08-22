#ifndef DRIVER_THERMISTORS_H
#define DRIVER_THERMISTORS_H
void therm_task(void *arg);
int therm_info(struct task_descriptor *task, char *dest);
#endif // DRIVER_THERMISTORS_H
