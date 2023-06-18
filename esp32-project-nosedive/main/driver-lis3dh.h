#ifndef DRIVER_LIS3DH_H
#define DRIVER_LIS3DH_H
void lis3dh_task(void *arg);
int lis3dh_info(struct task_descriptor *task, char *dest);
#endif // DRIVER_LIS3DH_H
