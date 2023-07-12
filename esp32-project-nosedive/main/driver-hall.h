#ifndef DRIVER_HALL_H
#define DRIVER_HALL_H
void hall_take_sample(void *dummy);
void hall_task(void *arg);
int hall_info(struct task_descriptor *task, char *dest);
#endif // DRIVER_HALL_H
